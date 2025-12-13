// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/quarantine.h"

#import <ApplicationServices/ApplicationServices.h>
#import <Foundation/Foundation.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/services/quarantine/common.h"
#include "components/services/quarantine/common_mac.h"
#include "url/gurl.h"

namespace quarantine {

namespace {

extern "C" {
Boolean MDItemSetAttribute(MDItemRef, CFStringRef, CFTypeRef);
}

// As of Mac OS X 10.4 ("Tiger"), files can be tagged with metadata describing
// various attributes. Metadata is integrated with the system's Spotlight
// feature and is searchable. Ordinarily, metadata can only be set by
// Spotlight importers, which requires that the importer own the target file.
// However, there's an attribute intended to describe the origin of a
// file, that can store the source URL and referrer of a downloaded file.
// It's stored as a "com.apple.metadata:kMDItemWhereFroms" extended attribute,
// structured as a binary1-format plist containing a list of sources. This
// attribute can only be populated by the downloader, not a Spotlight importer.
// Safari on 10.4 and later populates this attribute.
//
// With this metadata set, you can locate downloads by performing a Spotlight
// search for their source or referrer URLs, either from within the Spotlight
// UI or from the command line:
//     mdfind 'kMDItemWhereFroms == "http://releases.mozilla.org/*"'
//
// The most modern metadata API is NSMetadata, with this attribute available as
// `NSMetadataItemWhereFromsKey`, but NSMetadata offers a query-only interface.
// The original metadata API is the Metadata.framework in CoreServices, and
// while it too offers a query-only interface, at least it has a private call to
// set the metadata. Therefore, use Metadata.framework.
bool AddOriginMetadataToFile(const base::FilePath& file,
                             const GURL& source,
                             const GURL& referrer) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  NSString* file_path = base::apple::FilePathToNSString(file);
  if (!file_path) {
    return false;
  }

  base::apple::ScopedCFTypeRef<MDItemRef> md_item(
      MDItemCreate(kCFAllocatorDefault, base::apple::NSToCFPtrCast(file_path)));
  if (!md_item) {
    LOG(WARNING) << "MDItemCreate failed for path " << file.value();
    return false;
  }

  // We won't put any more than 2 items into the attribute.
  NSMutableArray* list = [NSMutableArray arrayWithCapacity:2];

  // Follow Safari's lead: the first item in the list is the source URL of the
  // downloaded file. If the referrer is known, store that, too. The URLs may be
  // empty (e.g. files downloaded in Incognito mode); don't add empty URLs.
  NSString* origin_url = base::SysUTF8ToNSString(source.spec());
  if (origin_url && origin_url.length) {
    [list addObject:origin_url];
  }
  NSString* referrer_url = base::SysUTF8ToNSString(referrer.spec());
  if (referrer_url && referrer_url.length) {
    [list addObject:referrer_url];
  }

  if (list.count) {
    return MDItemSetAttribute(md_item.get(), kMDItemWhereFroms,
                              base::apple::NSToCFPtrCast(list));
  }

  return true;
}

// Adds quarantine metadata to `file`, assuming it has already been quarantined
// by the OS. `source` is the source URL for the download, and `referrer` is the
// URL the user initiated the download from.
//
// The OS will automatically quarantine files due to the LSFileQuarantineEnabled
// entry in our Info.plist, but it knows relatively little about the files. This
// adds more information about the download to improve the UI shown by the OS
// when the users try to open the file.
//
// Note that as of macOS 12.4 this information is not stored by macOS, but a
// contact at Apple asked us to ensure it is continued to be set, as "the
// system still does try to consult and utilize the information to as part of
// local policy decisions." Therefore, this function is best-effort and does not
// return a success value.
void AddQuarantineMetadataToFile(const base::FilePath& file,
                                 const GURL& source,
                                 const GURL& referrer) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  NSMutableDictionary* properties = [GetQuarantineProperties(file) mutableCopy];
  if (!properties) {
    // If there are no quarantine properties, then either there was some error
    // or the file isn't quarantined (e.g. because the user set exclusions for
    // certain file types). Don't add any metadata, because that will cause the
    // file to be quarantined against the user's wishes.
    return;
  }

  // kLSQuarantineAgentNameKey, kLSQuarantineAgentBundleIdentifierKey, and
  // kLSQuarantineTimeStampKey are automatically set (see LSQuarantine.h), so
  // only values that the OS can't infer must be set here.

  if (!properties[base::apple::CFToNSPtrCast(kLSQuarantineTypeKey)]) {
    CFStringRef type = source.SchemeIsHTTPOrHTTPS()
                           ? kLSQuarantineTypeWebDownload
                           : kLSQuarantineTypeOtherDownload;
    properties[base::apple::CFToNSPtrCast(kLSQuarantineTypeKey)] =
        base::apple::CFToNSPtrCast(type);
  }

  if (!properties[base::apple::CFToNSPtrCast(kLSQuarantineDataURLKey)]) {
    properties[base::apple::CFToNSPtrCast(kLSQuarantineDataURLKey)] =
        base::SysUTF8ToNSString(source.spec());
  }

  if (!properties[base::apple::CFToNSPtrCast(kLSQuarantineOriginURLKey)] &&
      referrer.is_valid()) {
    properties[base::apple::CFToNSPtrCast(kLSQuarantineOriginURLKey)] =
        base::SysUTF8ToNSString(referrer.spec());
  }

  NSURL* file_url = base::apple::FilePathToNSURL(file);
  if (!file_url) {
    return;
  }

  NSError* error = nullptr;
  [file_url setResourceValue:properties
                      forKey:NSURLQuarantinePropertiesKey
                       error:&error];
}

}  // namespace

void QuarantineFile(const base::FilePath& file,
                    const GURL& source_url_unsafe,
                    const GURL& referrer_url_unsafe,
                    const std::optional<url::Origin>& request_initiator,
                    const std::string& client_guid,
                    mojom::Quarantine::QuarantineFileCallback callback) {
  if (!base::PathExists(file)) {
    std::move(callback).Run(QuarantineFileResult::FILE_MISSING);
    return;
  }

  GURL source_url = SanitizeUrlForQuarantine(source_url_unsafe);
  GURL referrer_url = SanitizeUrlForQuarantine(referrer_url_unsafe);
  if (source_url.is_empty() && request_initiator.has_value()) {
    source_url = SanitizeUrlForQuarantine(request_initiator->GetURL());
  }

  // Both origin and quarantine metadata are best-effort.
  AddOriginMetadataToFile(file, source_url, referrer_url);
  AddQuarantineMetadataToFile(file, source_url, referrer_url);

  std::move(callback).Run(QuarantineFileResult::OK);
}

}  // namespace quarantine
