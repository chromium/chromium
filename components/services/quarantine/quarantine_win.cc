// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/quarantine.h"

#include <objbase.h>

#include <shobjidl.h>
#include <windows.h>

#include <cguid.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wininet.h>
#include <wrl/client.h>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/uuid.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "components/services/quarantine/common.h"
#include "components/services/quarantine/common_win.h"
#include "url/gurl.h"

namespace quarantine {
namespace {

// Returns true for a valid |url| whose length does not exceed
// INTERNET_MAX_URL_LENGTH.
bool IsValidUrlForAttachmentServices(const GURL& url) {
  return url.is_valid() && url.spec().size() <= INTERNET_MAX_URL_LENGTH;
}

// Maps a return code from an unsuccessful IAttachmentExecute::Save() call to a
// QuarantineFileResult.
//
// Typical return codes from IAttachmentExecute::Save():
//   S_OK   : The file was okay. If any viruses were found, they were cleaned.
//   E_FAIL : Virus infected.
//   INET_E_SECURITY_PROBLEM : The file was blocked due to security policy.
//
// Any other return value indicates an unexpected error during the scan.
QuarantineFileResult FailedSaveResultToQuarantineResult(HRESULT result) {
  switch (result) {
    case INET_E_SECURITY_PROBLEM:  // 0x800c000e
      // This is returned if the download was blocked due to security
      // restrictions. E.g. if the source URL was in the Restricted Sites zone
      // and downloads are blocked on that zone, then the download would be
      // deleted and this error code is returned.
      return QuarantineFileResult::BLOCKED_BY_POLICY;

    case E_FAIL:  // 0x80004005
      // Returned if an anti-virus product reports an infection in the
      // downloaded file during IAE::Save().
      return QuarantineFileResult::VIRUS_INFECTED;

    default:
      // Any other error that occurs during IAttachmentExecute::Save() likely
      // indicates a problem with the security check, but not necessarily the
      // download. This also includes cases where SUCCEEDED(result) is true. In
      // the latter case we are likely dealing with a situation where the file
      // is missing after a successful scan. See http://crbug.com/153212.
      return QuarantineFileResult::SECURITY_CHECK_FAILED;
  }
}

// Invokes IAttachmentExecute::Save on CLSID_AttachmentServices to validate the
// downloaded file. The call may scan the file for viruses and if necessary,
// annotate it with evidence.  As a result of the validation, the file may be
// deleted. See: http://msdn.microsoft.com/en-us/bb776299
//
// IAE::Save() will delete the file if it was found to be blocked by local
// security policy or if it was found to be infected. The call may also delete
// the file due to other failures (http://crbug.com/153212). In these cases,
// |result| will contain the failure code.
//
// The return value is |false| iff the function fails to invoke
// IAttachmentExecute::Save(). If the function returns |true|, then the result
// of invoking IAttachmentExecute::Save() is stored in |result|.
//
// |full_path| : is the path to the downloaded file. This should be the final
//               path of the download. Must be present.
// |source_url|: the source URL for the download. If empty, the source will
//               be set to 'about:internet'.
// |referrer_url|: the referrer URL for the download. If empty, the referrer
//               will not be set.
// |client_guid|: the GUID to be set in the IAttachmentExecute client slot.
//                Used to identify the app to the system AV function.
// |result|: Receives the result of invoking IAttachmentExecute::Save().
bool InvokeAttachmentServices(const base::FilePath& full_path,
                              const GURL& source_url,
                              const GURL& referrer_url,
                              const GUID& client_guid,
                              QuarantineFileResult* result) {
  Microsoft::WRL::ComPtr<IAttachmentExecute> attachment_services;
  HRESULT hr = ::CoCreateInstance(CLSID_AttachmentServices, nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&attachment_services));

  if (FAILED(hr)) {
    // The thread must have COM initialized.
    DCHECK_NE(CO_E_NOTINITIALIZED, hr);
    return false;
  }

  // Note that it is mandatory to check the return values from here on out. If
  // setting one of the parameters fails, it could leave the object in a state
  // where the final Save() call will also fail.

  hr = attachment_services->SetClientGuid(client_guid);
  if (FAILED(hr))
    return false;

  hr = attachment_services->SetLocalPath(full_path.value().c_str());
  if (FAILED(hr))
    return false;

  // The source URL could be empty if it was not a valid URL, or was not HTTP/S,
  // or the download was off-the-record. If so, use "about:internet" as a
  // fallback URL. The latter is known to reliably map to the Internet zone.
  //
  // In addition, URLs that are longer than INTERNET_MAX_URL_LENGTH are also
  // known to cause problems for URLMon. Hence also use "about:internet" in
  // these cases. See http://crbug.com/601538.
  hr = attachment_services->SetSource(
      IsValidUrlForAttachmentServices(source_url)
          ? base::UTF8ToWide(source_url.spec()).c_str()
          : L"about:internet");
  if (FAILED(hr))
    return false;

  // Only set referrer if one is present and shorter than
  // INTERNET_MAX_URL_LENGTH. Also, the source_url is authoritative for
  // determining the relative danger of |full_path| so we don't consider it an
  // error if we have to skip the |referrer_url|.
  if (IsValidUrlForAttachmentServices(referrer_url)) {
    hr = attachment_services->SetReferrer(
        base::UTF8ToWide(referrer_url.spec()).c_str());
    if (FAILED(hr))
      return false;
  }

  HRESULT save_result = S_OK;
  {
    // This method has been known to take longer than 10 seconds in some
    // instances.
    SCOPED_UMA_HISTOGRAM_LONG_TIMER("Download.AttachmentServices.Duration");
    save_result = attachment_services->Save();
  }

  // If the download file is missing after the call, then treat this as an
  // interrupted download.
  //
  // If IAttachmentExecute::Save() failed, but the downloaded file is still
  // around, then don't interrupt the download. Attachment Execution Services
  // deletes the submitted file if the downloaded file is blocked by policy or
  // if it was found to be infected.
  //
  // If the file is still there, then the error could be due to Windows
  // Attachment Services not being available or some other error during the AES
  // invocation. In either case, we don't surface the error to the user.
  *result = base::PathExists(full_path)
                ? QuarantineFileResult::OK
                : FailedSaveResultToQuarantineResult(save_result);

  return true;
}

}  // namespace

// Sets the Zone Identifier on the file to "Internet" (3). Returns true if the
// function succeeds, false otherwise. A failure is expected if alternate
// streams are not supported, like a file on a FAT32 filesystem.  This function
// does not invoke Windows Attachment Execution Services.
//
// On Windows 10 or higher, the ReferrerUrl and HostUrl values are set according
// to the behavior of the IAttachmentExecute interface.
//
// |full_path| is the path to the downloaded file.
QuarantineFileResult SetInternetZoneIdentifierDirectly(
    const base::FilePath& full_path,
    const GURL& source_url,
    const GURL& referrer_url) {
  const DWORD kShare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  std::wstring path = full_path.value() + kZoneIdentifierStreamSuffix;
  base::win::ScopedHandle file(::CreateFile(path.c_str(), GENERIC_WRITE, kShare,
                                            nullptr, OPEN_ALWAYS,
                                            FILE_ATTRIBUTE_NORMAL, nullptr));
  if (!file.IsValid())
    return QuarantineFileResult::ANNOTATION_FAILED;

  static const char kReferrerUrlFormat[] = "ReferrerUrl=%s\r\n";
  static const char kHostUrlFormat[] = "HostUrl=%s\r\n";

  std::string identifier = "[ZoneTransfer]\r\nZoneId=3\r\n";
  // Match what the InvokeAttachmentServices() function will output, including
  // the order of the values.
  if (IsValidUrlForAttachmentServices(referrer_url)) {
    identifier.append(
        base::StringPrintf(kReferrerUrlFormat, referrer_url.spec().c_str()));
  }
  identifier.append(base::StringPrintf(
      kHostUrlFormat, IsValidUrlForAttachmentServices(source_url)
                          ? source_url.spec().c_str()
                          : "about:internet"));

  // Don't include trailing null in data written.
  DWORD written = 0;
  BOOL write_result = ::WriteFile(file.Get(), identifier.c_str(),
                                  identifier.length(), &written, nullptr);
  BOOL flush_result = FlushFileBuffers(file.Get());

  return write_result && flush_result && written == identifier.length()
             ? QuarantineFileResult::OK
             : QuarantineFileResult::ANNOTATION_FAILED;
}

void QuarantineFile(const base::FilePath& file,
                    const GURL& source_url_unsafe,
                    const GURL& referrer_url_unsafe,
                    const std::optional<url::Origin>& request_initiator,
                    const std::string& client_guid,
                    mojom::Quarantine::QuarantineFileCallback callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  int64_t file_size = 0;
  if (!base::PathExists(file) || !base::GetFileSize(file, &file_size)) {
    std::move(callback).Run(QuarantineFileResult::FILE_MISSING);
    return;
  }

  std::string braces_guid = "{" + client_guid + "}";
  GUID guid = GUID_NULL;
  if (base::Uuid::ParseCaseInsensitive(client_guid).is_valid()) {
    HRESULT hr = CLSIDFromString(base::UTF8ToWide(braces_guid).c_str(), &guid);
    if (FAILED(hr))
      guid = GUID_NULL;
  }

  GURL source_url = SanitizeUrlForQuarantine(source_url_unsafe);
  if (source_url.is_empty() && request_initiator.has_value()) {
    source_url = SanitizeUrlForQuarantine(request_initiator->GetURL());
  }

  GURL referrer_url = SanitizeUrlForQuarantine(referrer_url_unsafe);

  if (file_size == 0 || IsEqualGUID(guid, GUID_NULL)) {
    // Calling InvokeAttachmentServices on an empty file can result in the file
    // being deleted.  Also an anti-virus scan doesn't make a lot of sense to
    // perform on an empty file.
    std::move(callback).Run(
        SetInternetZoneIdentifierDirectly(file, source_url, referrer_url));
    return;
  }

  QuarantineFileResult attachment_services_result = QuarantineFileResult::OK;
  if (InvokeAttachmentServices(file, source_url, referrer_url, guid,
                               &attachment_services_result)) {
    std::move(callback).Run(attachment_services_result);
    return;
  }

  std::move(callback).Run(
      SetInternetZoneIdentifierDirectly(file, source_url, referrer_url));
}

}  // namespace quarantine
