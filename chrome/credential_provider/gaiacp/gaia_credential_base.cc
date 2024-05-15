// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of CGaiaCredentialBase class, used as the base for all
// credentials that need to show the gaia sign in page.

#include "chrome/credential_provider/gaiacp/gaia_credential_base.h"

#include <ntstatus.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/win/current_module.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_handle.h"
#include "build/branding_buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/associated_user_validator.h"
#include "chrome/credential_provider/gaiacp/auth_utils.h"
#include "chrome/credential_provider/gaiacp/device_policies_manager.h"
#include "chrome/credential_provider/gaiacp/event_logs_upload_manager.h"
#include "chrome/credential_provider/gaiacp/experiments_fetcher.h"
#include "chrome/credential_provider/gaiacp/experiments_manager.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/gaia_resources.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/gem_device_details_manager.h"
#include "chrome/credential_provider/gaiacp/grit/gaia_static_resources.h"
#include "chrome/credential_provider/gaiacp/internet_availability_checker.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/os_gaia_user_manager.h"
#include "chrome/credential_provider/gaiacp/os_process_manager.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "chrome/credential_provider/gaiacp/password_recovery_manager.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/gaiacp/scoped_lsa_policy.h"
#include "chrome/credential_provider/gaiacp/scoped_user_profile.h"
#include "chrome/credential_provider/gaiacp/user_policies_manager.h"
#include "chrome/credential_provider/gaiacp/win_http_url_fetcher.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "content/public/common/content_switches.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/re2/src/re2/re2.h"

namespace credential_provider {

namespace {

constexpr wchar_t kPermittedAccounts[] = L"permitted_accounts";
constexpr wchar_t kPermittedAccountsSeparator[] = L",";
constexpr char kGetAccessTokenBodyWithScopeFormat[] =
    "client_id=%s&"
    "client_secret=%s&"
    "grant_type=refresh_token&"
    "refresh_token=%s&"
    "scope=%s";
constexpr wchar_t kRegCloudAssociation[] = L"enable_cloud_association";
// The access scopes should be separated by single space.
constexpr char kAccessScopes[] =
    "https://www.googleapis.com/auth/admin.directory.user";
constexpr int kHttpTimeout = 3000;  // in milliseconds

// Names of keys used to fetch the custom attributes from google admin sdk
// users directory api.
constexpr char kKeyCustomSchemas[] = "customSchemas";
constexpr char kKeyEnhancedDesktopSecurity[] = "Enhanced_desktop_security";
constexpr char kKeyADAccounts[] = "AD_accounts";
constexpr char kKeyLocalWindowsAccounts[] = "Local_Windows_accounts";

// List of errors where Windows returns during password change that can't be
// worked out with manual user input during forgot password flow.
constexpr UINT kPasswordErrors[] = {IDS_PASSWORD_COMPLEXITY_ERROR_BASE,
                                    IDS_USER_NOT_FOUND_PASSWORD_ERROR_BASE,
                                    IDS_AD_PASSWORD_CHANGE_DENIED_BASE};

std::vector<std::wstring> GetPermittedAccounts() {
  std::wstring permitted_accounts_reg =
      GetGlobalFlagOrDefault(kPermittedAccounts, L"");

  return base::SplitString(base::ToLowerASCII(permitted_accounts_reg),
                           kPermittedAccountsSeparator,
                           base::WhitespaceHandling::TRIM_WHITESPACE,
                           base::SplitResult::SPLIT_WANT_NONEMPTY);
}

std::wstring GetEmailDomains(const std::wstring restricted_domains_reg_key) {
  return GetGlobalFlagOrDefault(restricted_domains_reg_key, L"");
}

std::wstring GetEmailDomains() {
  if (DevicePoliciesManager::Get()->CloudPoliciesEnabled()) {
    DevicePolicies device_policies;
    DevicePoliciesManager::Get()->GetDevicePolicies(&device_policies);
    return device_policies.GetAllowedDomainsStr();
  }

  // TODO (crbug.com/1135458): Clean up directly reading from registry after
  // cloud policies is launched.
  std::wstring email_domains_reg = GetEmailDomains(kEmailDomainsKey);
  std::wstring email_domains_reg_new = GetEmailDomains(kEmailDomainsKeyNew);
  return email_domains_reg.empty() ? email_domains_reg_new : email_domains_reg;
}

std::vector<std::wstring> GetEmailDomainsList() {
  return base::SplitString(base::ToLowerASCII(GetEmailDomains()),
                           kPermittedAccountsSeparator,
                           base::WhitespaceHandling::TRIM_WHITESPACE,
                           base::SplitResult::SPLIT_WANT_NONEMPTY);
}

// Use WinHttpUrlFetcher to communicate with the admin sdk and fetch the active
// directory samAccountName if available and list of local account name mapping
// configured as custom attributes.
HRESULT GetExistingAccountMappingFromCD(
    const std::wstring& email,
    const std::string& access_token,
    std::string* sam_account_name,
    std::vector<std::string>* local_account_names,
    BSTR* error_text) {
  LOGFN(VERBOSE);
  DCHECK(email.size() > 0);
  DCHECK(access_token.size() > 0);
  DCHECK(sam_account_name);
  DCHECK(local_account_names);
  DCHECK(error_text);
  *error_text = nullptr;

  std::string escape_url_encoded_email =
      base::EscapeUrlEncodedData(base::WideToUTF8(email), true);
  std::string get_cd_user_url = base::StringPrintf(
      "https://www.googleapis.com/admin/directory/v1/users/"
      "%s?projection=full&viewType=domain_public",
      escape_url_encoded_email.c_str());
  LOGFN(VERBOSE) << "Encoded URL : " << get_cd_user_url;
  auto fetcher = WinHttpUrlFetcher::Create(GURL(get_cd_user_url));
  fetcher->SetRequestHeader("Accept", "application/json");
  fetcher->SetHttpRequestTimeout(kHttpTimeout);

  std::string access_token_header =
      base::StringPrintf("Bearer %s", access_token.c_str());
  fetcher->SetRequestHeader("Authorization", access_token_header.c_str());
  std::vector<char> cd_user_response;
  HRESULT hr = fetcher->Fetch(&cd_user_response);
  std::string cd_user_response_json_string =
      std::string(cd_user_response.begin(), cd_user_response.end());
  if (FAILED(hr)) {
    LOGFN(ERROR) << "fetcher->Fetch hr=" << putHR(hr);
    *error_text =
        CGaiaCredentialBase::AllocErrorString(IDS_INTERNAL_ERROR_BASE);
    return hr;
  }

  std::vector<std::string> sam_account_names;
  hr = SearchForListInStringDictUTF8(
      "value", cd_user_response_json_string,
      {kKeyCustomSchemas, kKeyEnhancedDesktopSecurity, kKeyADAccounts},
      &sam_account_names);

  // Note: We only consider the first sam_account_name right now.
  // We will expand this to consider all account names listed in the
  // multi-value and perform username resolution in the future.
  if (sam_account_names.size() > 0) {
    *sam_account_name = sam_account_names.at(0);
  }

  hr = SearchForListInStringDictUTF8(
      "value", cd_user_response_json_string,
      {kKeyCustomSchemas, kKeyEnhancedDesktopSecurity,
       kKeyLocalWindowsAccounts},
      local_account_names);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "Attempt to parse localAccountInfo failed.";
  }

  return hr;
}

// Request a downscoped access token using the refresh token provided in the
// input.
HRESULT RequestDownscopedAccessToken(const std::string& refresh_token,
                                     std::string* access_token,
                                     BSTR* error_text) {
  DCHECK(refresh_token.size() > 0);
  DCHECK(access_token);
  DCHECK(error_text);
  *error_text = nullptr;

  GaiaUrls* gaia_urls = GaiaUrls::GetInstance();
  std::string enc_client_id =
      base::EscapeUrlEncodedData(gaia_urls->oauth2_chrome_client_id(), true);
  std::string enc_client_secret = base::EscapeUrlEncodedData(
      gaia_urls->oauth2_chrome_client_secret(), true);
  std::string enc_refresh_token =
      base::EscapeUrlEncodedData(refresh_token, true);
  std::string get_access_token_body = base::StringPrintf(
      kGetAccessTokenBodyWithScopeFormat, enc_client_id.c_str(),
      enc_client_secret.c_str(), enc_refresh_token.c_str(),
      base::EscapeUrlEncodedData(kAccessScopes, true).c_str());
  std::string get_oauth_token_url =
      base::StringPrintf("%s", gaia_urls->oauth2_token_url().spec().c_str());

  auto oauth_fetcher = WinHttpUrlFetcher::Create(GURL(get_oauth_token_url));
  oauth_fetcher->SetRequestBody(get_access_token_body.c_str());
  oauth_fetcher->SetRequestHeader("content-type",
                                  "application/x-www-form-urlencoded");
  oauth_fetcher->SetHttpRequestTimeout(kHttpTimeout);

  std::vector<char> oauth_response;
  HRESULT oauth_hr = oauth_fetcher->Fetch(&oauth_response);
  if (FAILED(oauth_hr)) {
    LOGFN(ERROR) << "oauth_fetcher.Fetch hr=" << putHR(oauth_hr);
    *error_text =
        CGaiaCredentialBase::AllocErrorString(IDS_INTERNAL_ERROR_BASE);
    return oauth_hr;
  }

  std::string oauth_response_json_string =
      std::string(oauth_response.begin(), oauth_response.end());
  *access_token = SearchForKeyInStringDictUTF8(oauth_response_json_string,
                                               {kKeyAccessToken});
  if (access_token->empty()) {
    LOGFN(ERROR) << "Fetched access token with new scopes is empty.";
    *error_text =
        CGaiaCredentialBase::AllocErrorString(IDS_EMPTY_ACCESS_TOKEN_BASE);
    return E_FAIL;
  }
  return S_OK;
}

HRESULT GetUserAndDomainInfo(
    const std::string& sam_account_name,
    const std::vector<std::string>& local_account_names,
    std::wstring* existing_sid,
    BSTR* error_text) {
  std::wstring user_name;
  std::wstring domain_name;
  OSUserManager* os_user_manager = OSUserManager::Get();
  DCHECK(os_user_manager);

  bool is_ad_user =
      os_user_manager->IsDeviceDomainJoined() && !sam_account_name.empty();
  LOGFN(VERBOSE) << "is_ad_user=" << is_ad_user;
  // Login via existing AD account mapping when the device is domain joined if
  // the AD account mapping is available.
  if (is_ad_user) {
    // The format for ad_upn custom attribute is domainName\\userName.
    // Note that admin configures it as "domainName\userName" but admin
    // sdk stores it with another escape backslash character in it leading
    // multiple backslashes.
    const wchar_t kSlashDelimiter[] = L"\\";
    std::vector<std::wstring> tokens =
        base::SplitString(base::UTF8ToWide(sam_account_name), kSlashDelimiter,
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    // Values fetched from custom attribute shouldn't be empty.
    if (tokens.size() != 2) {
      LOGFN(ERROR) << "Found unparseable samAccountName in cloud directory : "
                   << sam_account_name;
      *error_text =
          CGaiaCredentialBase::AllocErrorString(IDS_INVALID_AD_UPN_BASE);
      return E_FAIL;
    }

    domain_name = tokens.at(0);
    user_name = tokens.at(1);
  } else {
    // Fallback to using local account mapping for all other scenarios.

    // Step 1: Filter out invalid local account names based on serial number
    // etc. The mapping would look like "un:abcd,sn:1234" where "un" represents
    // the local user name and "sn" represents the serial number of the device.
    // Note that "sn" is optional, but it is recommended to be used by the IT
    // admin.

    // The variable that holds all the local accounts which has
    // a matching serial number of the device.
    std::vector<std::wstring> filtered_local_account_names;

    // The variable that holds all the local accounts which doesn't have
    // any serial_number mapping in custom attributes.
    std::vector<std::wstring> filtered_local_account_names_no_sn;

    for (auto local_account_name : local_account_names) {
      LOGFN(VERBOSE) << "CD local account name : " << local_account_name;
      // The format for local_account_name custom attribute is
      // "un:abcd,sn:1234" where "un:abcd" would always exist and "sn:1234" is
      // optional.
      std::string username;
      std::string serial_number;
      // Note: "?:" is used to signify non-capturing groups. For more details,
      // look at https://github.com/google/re2/wiki/Syntax link.
      re2::RE2::FullMatch(local_account_name, "un:([^,]+)(?:,sn:([^,]+))?",
                          &username, &serial_number);

      // Only collect those user names that exist on the windows device.
      std::wstring ignore;
      HRESULT hr = os_user_manager->GetUserSID(
          OSUserManager::GetLocalDomain().c_str(),
          base::UTF8ToWide(username).c_str(), &ignore);
      if (FAILED(hr))
        continue;

      LOGFN(VERBOSE) << "RE2 username : " << username;
      LOGFN(VERBOSE) << "RE2 serial_number : " << serial_number;

      if (!username.empty() && !serial_number.empty()) {
        std::string device_serial_number =
            base::WideToUTF8(GetSerialNumber().c_str());
        LOGFN(VERBOSE) << "Device serial_number : " << device_serial_number;
        if (base::EqualsCaseInsensitiveASCII(serial_number,
                                             device_serial_number))
          filtered_local_account_names.push_back(base::UTF8ToWide(username));
      } else if (!username.empty()) {
        filtered_local_account_names_no_sn.push_back(
            base::UTF8ToWide(username));
      }
    }

    // Step 2: If more than one mapping found on both the above lists
    // OR no mapping found on either one of them, then return NTE_NOT_FOUND.
    if (filtered_local_account_names.size() != 1 &&
        filtered_local_account_names_no_sn.size() != 1) {
      return NTE_NOT_FOUND;
    }

    // Step 3: Assign the extracted user name to user_name variable so that we
    // can verify for existence of SID on the device with the extracted user
    // name on the current windows device.
    user_name = filtered_local_account_names.size() == 1
                    ? filtered_local_account_names.at(0)
                    : filtered_local_account_names_no_sn.at(0);
    domain_name = OSUserManager::GetLocalDomain();
  }

  LOGFN(VERBOSE) << "Get user sid for user " << user_name << " and domain name "
                 << domain_name;
  HRESULT hr = os_user_manager->GetUserSID(domain_name.c_str(),
                                           user_name.c_str(), existing_sid);

  if (existing_sid->length() > 0) {
    LOGFN(VERBOSE) << "Found existing SID = " << *existing_sid;
    return S_OK;
  }

  LOGFN(ERROR) << "No existing sid found with user name : " << user_name
               << " and domain name: " << domain_name << ". hr=" << putHR(hr);

  if (is_ad_user) {
    *error_text =
        CGaiaCredentialBase::AllocErrorString(IDS_INVALID_AD_UPN_BASE);
    LOGFN(ERROR) << "Could not find a valid samAccountName.";
    return E_FAIL;
  }

  // For non-AD usecase, we will fallback to creating new local account
  // instead of failing the login attempt.
  return NTE_NOT_FOUND;
}

// Find an existing account associated with GCPW user if one exists.
// (1) Verifies if the gaia user has a corresponding mapping in Google
//   Admin SDK Users Directory and contains the custom_schema that contains
//   the sam_account_name or local_user_info for the corresponding user.
// (2) If there is an entry in cloud directory, gcpw would search for the SID
//   corresponding to that user entry on the device.
// (3) If a SID is found, then it would log the user onto the device using
//   username extracted from Google Admin SDK Users Directory and password
//   being the same as the gaia entity.
// (4) If there is no entry found in cloud directory, gcpw would fallback to
//   create a new local user on the device.
//
// Below are the scenarios where we fallback to create a new local user:
// (1) No mapping available in user's cloud directory custom schema attributes.
// (2) If a local user mapping exists but the extracted domainname/username
//     combination doesn't have a valid SID.
//
// Below are the failure scenarios :
// (1) Failed getting a downscoped access token from refresh token.
// (2) If communication with cloud directory fails, then we fail the login.
// (3) If an attempt to find SID from domain controller or local machine failed,
//     then we fail the login.
// (4) Parsing the samAccountName or localAccountInfo failed.
// (5) If an AD user mapping exists but the extracted domainname/username
//     combination doesn't have a valid SID.
HRESULT FindExistingUserSidIfAvailable(const std::string& refresh_token,
                                       const std::wstring& email,
                                       wchar_t* sid,
                                       const DWORD sid_length,
                                       BSTR* error_text) {
  DCHECK(sid);
  DCHECK(error_text);
  *error_text = nullptr;

  // Step 1: Get the downscoped access token with required admin sdk scopes.
  std::string access_token;
  HRESULT hr =
      RequestDownscopedAccessToken(refresh_token, &access_token, error_text);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "RequestDownscopedAccessToken hr=" << putHR(hr);
    return hr;
  }

  // Step 2: Make a get call to admin sdk using the fetched access_token and
  // retrieve the sam_account_name.
  std::string sam_account_name;
  std::vector<std::string> local_account_names;
  hr = GetExistingAccountMappingFromCD(email, access_token, &sam_account_name,
                                       &local_account_names, error_text);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetExistingAccountMappingFromCD hr=" << putHR(hr);
    return hr;
  }
  LOGFN(VERBOSE) << "sam_account_name=" << sam_account_name;

  std::wstring existing_sid = std::wstring();
  hr = GetUserAndDomainInfo(sam_account_name, local_account_names,
                            &existing_sid, error_text);

  if (SUCCEEDED(hr))
    wcscpy_s(sid, sid_length, existing_sid.c_str());

  return hr;
}

// Tries to find a user associated to the gaia_id stored in |result| under the
// key |kKeyId|. If one exists, then this function will fill out |gaia_id|,
// |username|, |domain| and |sid| with the user's information. If not this
// function will try to generate a new username derived from the email and fill
// out only |gaia_id| and |username|. |domain| will always be the local domain
// since only local users can be created. |sid| will be empty until the user is
// created later on. |is_consumer_account| will be set to true if the email used
// to sign in is gmail or googlemail.
HRESULT MakeUsernameForAccount(const base::Value::Dict& result,
                               std::wstring* gaia_id,
                               wchar_t* username,
                               DWORD username_length,
                               wchar_t* domain,
                               DWORD domain_length,
                               wchar_t* sid,
                               DWORD sid_length,
                               bool* is_consumer_account,
                               BSTR* error_text) {
  DCHECK(gaia_id);
  DCHECK(username);
  DCHECK(domain);
  DCHECK(sid);
  DCHECK(is_consumer_account);
  DCHECK(error_text);

  // Determine if the email is a consumer domain (gmail.com or googlemail.com).
  std::wstring email = GetDictString(result, kKeyEmail);
  base::ranges::transform(email, email.begin(), ::tolower);
  std::wstring::size_type consumer_domain_pos = email.find(L"@gmail.com");
  if (consumer_domain_pos == std::wstring::npos)
    consumer_domain_pos = email.find(L"@googlemail.com");

  *is_consumer_account = consumer_domain_pos != std::wstring::npos;

  *gaia_id = GetDictString(result, kKeyId);

  // First try to detect if this gaia account has been used to create an OS
  // user already.  If so, return the OS username of that user.
  HRESULT hr = GetSidFromId(*gaia_id, sid, sid_length);
  if (FAILED(hr)) {
    LOGFN(VERBOSE) << "Failed fetching Sid from Id : " << putHR(hr);
    // If there is no gaia id user property available in the registry,
    // fallback to email address mapping.
    hr = GetSidFromEmail(email, sid, sid_length);
    if (FAILED(hr))
      LOGFN(VERBOSE) << "Failed fetching Sid from email : " << putHR(hr);
  }

  // Check if the machine is domain joined and get the domain name if domain
  // joined.
  if (SUCCEEDED(hr)) {
    // This makes sure that we don't invoke the network calls on every login
    // attempt and instead fallback to the SID to gaia id mapping created by
    // GCPW.
    LOGFN(VERBOSE) << "Found existing SID created in GCPW registry entry = "
                   << sid;

    hr = OSUserManager::Get()->FindUserBySidWithFallback(
        sid, username, username_length, domain, domain_length);
    if (FAILED(hr)) {
      *error_text =
          CGaiaCredentialBase::AllocErrorString(IDS_INVALID_AD_UPN_BASE);
    }
    return hr;

  } else if (CGaiaCredentialBase::IsCloudAssociationEnabled()) {
    LOGFN(VERBOSE) << "Lookup cloud association.";

    std::string refresh_token = GetDictStringUTF8(result, kKeyRefreshToken);
    hr = FindExistingUserSidIfAvailable(refresh_token, email, sid, sid_length,
                                        error_text);

    if (SUCCEEDED(hr)) {
      hr = OSUserManager::Get()->FindUserBySidWithFallback(
          sid, username, username_length, domain, domain_length);
      if (FAILED(hr)) {
        *error_text =
            CGaiaCredentialBase::AllocErrorString(IDS_INVALID_AD_UPN_BASE);
      }
      return hr;
    } else if (hr == NTE_NOT_FOUND) {
      LOGFN(ERROR) << "No valid sid mapping found."
                   << "Fallback to create a new local user account. hr="
                   << putHR(hr);
    } else if (FAILED(hr)) {
      LOGFN(ERROR) << "Failed finding existing user sid for GCPW user. hr="
                   << putHR(hr);
      return hr;
    }

  } else {
    LOGFN(VERBOSE) << "Fallback to create a new local user account";
  }

  LOGFN(VERBOSE) << "No existing user found associated to gaia id:" << *gaia_id;
  wcscpy_s(domain, domain_length, OSUserManager::GetLocalDomain().c_str());
  username[0] = 0;
  sid[0] = 0;

  // Create a username based on the email address.  Usernames are more
  // restrictive than emails, so some transformations are needed.  This tries
  // to preserve the email as much as possible in the username while respecting
  // Windows username rules.  See remarks in
  // https://docs.microsoft.com/en-us/windows/desktop/api/lmaccess/ns-lmaccess-_user_info_0
  std::wstring os_username = email;

  // If the email is a consumer domain, strip it.
  if (consumer_domain_pos != std::wstring::npos) {
    os_username.resize(consumer_domain_pos);
  } else {
    // Strip off well known TLDs.
    std::string username_utf8 =
        gaia::SanitizeEmail(base::WideToUTF8(os_username));

    if (GetGlobalFlagOrDefault(kRegUseShorterAccountName, 0)) {
      size_t separator_pos = username_utf8.find('@');

      // os_username carries the email. Fall through if not find "@" in the
      // email.
      if (separator_pos != username_utf8.npos) {
        username_utf8 = username_utf8.substr(0, separator_pos);
        os_username = base::UTF8ToWide(username_utf8);
      }
    } else {
      size_t tld_length =
          net::registry_controlled_domains::GetCanonicalHostRegistryLength(
              gaia::ExtractDomainName(username_utf8),
              net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
              net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

      // If an TLD is found strip it off, plus 1 to remove the separating dot
      // too.
      if (tld_length > 0) {
        username_utf8.resize(username_utf8.length() - tld_length - 1);
        os_username = base::UTF8ToWide(username_utf8);
      }
    }
  }

  // If the username is longer than 20 characters, truncate.
  if (os_username.size() > kWindowsUsernameBufferLength - 1)
    os_username.resize(kWindowsUsernameBufferLength - 1);

  // After resizing the os user name above, the last char may be a '.' which is
  // illegal as the last character per Microsoft documentation.
  // https://docs.microsoft.com/en-us/windows/win32/api/lmaccess/ns-lmaccess-user_info_1#remarks
  if (os_username.size() > 0 && os_username.back() == '.')
    os_username.resize(os_username.size() - 1);

  // Replace invalid characters.  While @ is not strictly invalid according to
  // MSDN docs, it causes trouble.
  for (auto& c : os_username) {
    if (wcschr(L"@\\[]:|<>+=;?*", c) != nullptr || c < 32)
      c = L'_';
  }

  wcscpy_s(username, username_length, os_username.c_str());

  return S_OK;
}

// Waits for the login UI to complete and returns the result of the operation.
// This function returns S_OK on success, E_UNEXPECTED on failure, and E_ABORT
// if the user aborted or timed out (or was killed during cleanup).
HRESULT WaitForLoginUIAndGetResult(
    CGaiaCredentialBase::UIProcessInfo* uiprocinfo,
    std::string* json_result,
    DWORD* exit_code,
    BSTR* status_text) {
  LOGFN(VERBOSE);
  DCHECK(uiprocinfo);
  DCHECK(json_result);
  DCHECK(exit_code);
  DCHECK(status_text);

  // Buffer used to accumulate output from UI.
  constexpr int kBufferSize = 4096;
  std::array<char, kBufferSize> output_buffer = {};
  absl::Cleanup zero_buffer_on_exit = [&output_buffer] {
    SecurelyClearBuffer(output_buffer.data(), output_buffer.size());
  };

  HRESULT hr = WaitForProcess(uiprocinfo->procinfo.process_handle(),
                              uiprocinfo->parent_handles, exit_code,
                              output_buffer.data(), output_buffer.size());
  // output_buffer contains sensitive information like the password. Don't log
  // it.
  LOGFN(VERBOSE) << "exit_code=" << *exit_code;

  // Killed internally in the GLS or killed externally by selecting
  // another credential while GLS is running.
  if (*exit_code == kUiecAbort || *exit_code == kUiecKilled) {
    LOGFN(WARNING) << "Aborted hr=" << putHR(hr);
    return E_ABORT;
  } else if (*exit_code != kUiecSuccess) {
    LOGFN(ERROR) << "Error hr=" << putHR(hr);
    *status_text =
        CGaiaCredentialBase::AllocErrorString(IDS_INVALID_UI_RESPONSE_BASE);
    return E_FAIL;
  }

  *json_result = std::string(output_buffer.data());
  return S_OK;
}

// This function validates the response from GLS and makes sure it contained
// all the fields required to proceed with logon.  This does not necessarily
// guarantee that the logon will succeed, only that GLS response seems correct.
HRESULT ValidateResult(const base::Value::Dict& result, BSTR* status_text) {
  DCHECK(status_text);

  // Check the exit_code to see if any errors were detected by the GLS.
  std::optional<int> exit_code = result.FindInt(kKeyExitCode);
  if (exit_code.value() != kUiecSuccess) {
    switch (exit_code.value()) {
      case kUiecAbort:
        // This case represents a user abort and no error message is shown.
        return E_ABORT;
      case kUiecTimeout:
      case kUiecKilled:
        NOTREACHED_IN_MIGRATION() << "Internal codes, not returned by GLS";
        break;
      case kUiecEMailMissmatch:
        *status_text =
            CGaiaCredentialBase::AllocErrorString(IDS_EMAIL_MISMATCH_BASE);
        break;
      case kUiecInvalidEmailDomain:
        *status_text = CGaiaCredentialBase::AllocErrorString(
            IDS_INVALID_EMAIL_DOMAIN_BASE);
        break;
      case kUiecMissingSigninData:
        *status_text =
            CGaiaCredentialBase::AllocErrorString(IDS_INVALID_UI_RESPONSE_BASE);
        break;
    }
    return E_FAIL;
  }

  // Check that the webui returned all expected values.

  bool has_error = false;
  std::string email = GetDictStringUTF8(result, kKeyEmail);
  if (email.empty()) {
    LOGFN(ERROR) << "Email is empty";
    has_error = true;
  }

  std::string fullname = GetDictStringUTF8(result, kKeyFullname);
  if (fullname.empty()) {
    LOGFN(ERROR) << "Full name is empty";
    has_error = true;
  }

  std::string id = GetDictStringUTF8(result, kKeyId);
  if (id.empty()) {
    LOGFN(ERROR) << "Id is empty";
    has_error = true;
  }

  std::string mdm_id_token = GetDictStringUTF8(result, kKeyMdmIdToken);
  if (mdm_id_token.empty()) {
    LOGFN(ERROR) << "mdm id token is empty";
    has_error = true;
  }

  std::string access_token = GetDictStringUTF8(result, kKeyAccessToken);
  if (access_token.empty()) {
    LOGFN(ERROR) << "access token is empty";
    has_error = true;
  }

  std::string password = GetDictStringUTF8(result, kKeyPassword);
  if (password.empty()) {
    LOGFN(ERROR) << "Password is empty";
    has_error = true;
  } else {
    SecurelyClearString(password);
  }

  std::string refresh_token = GetDictStringUTF8(result, kKeyRefreshToken);
  if (refresh_token.empty()) {
    LOGFN(ERROR) << "refresh token is empty";
    has_error = true;
  }

  std::string token_handle = GetDictStringUTF8(result, kKeyTokenHandle);
  if (token_handle.empty()) {
    LOGFN(ERROR) << "Token handle is empty";
    has_error = true;
  }

  if (has_error) {
    *status_text =
        CGaiaCredentialBase::AllocErrorString(IDS_INVALID_UI_RESPONSE_BASE);
    return E_UNEXPECTED;
  }

  return S_OK;
}

// If GCPW user policies or experiments are stale, make sure to fetch them
// before proceeding with the login.
void GetUserConfigsIfStale(const std::wstring& sid,
                           const std::wstring& gaia_id,
                           const std::wstring& access_token) {
  if (UserPoliciesManager::Get()->CloudPoliciesEnabled() &&
      UserPoliciesManager::Get()->IsUserPolicyStaleOrMissing(sid)) {
    // Save gaia id since it is needed for the cloud policies server request.
    HRESULT hr = SetUserProperty(sid, kUserId, gaia_id);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "SetUserProperty(id) hr=" << putHR(hr);
    }

    hr = UserPoliciesManager::Get()->FetchAndStoreCloudUserPolicies(
        sid, base::WideToUTF8(access_token));
    if (FAILED(hr)) {
      LOGFN(ERROR) << "Failed fetching user policies for user " << sid
                   << " Error: " << putHR(hr);
    }
  }

  if (ExperimentsManager::Get()->ExperimentsEnabled() &&
      GetTimeDeltaSinceLastFetch(sid, kLastUserExperimentsRefreshTimeRegKey) >
          kMaxTimeDeltaSinceLastExperimentsFetch) {
    HRESULT hr = SetUserProperty(sid, kUserId, gaia_id);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "SetUserProperty(id) hr=" << putHR(hr);
    }

    hr = ExperimentsFetcher::Get()->FetchAndStoreExperiments(
        sid, base::WideToUTF8(access_token));
    if (FAILED(hr)) {
      LOGFN(ERROR) << "Failed fetching experiments for user " << sid
                   << " Error: " << putHR(hr);
    }
  }
}

}  // namespace

CGaiaCredentialBase::UIProcessInfo::UIProcessInfo() {}

CGaiaCredentialBase::UIProcessInfo::~UIProcessInfo() {}

// static
bool CGaiaCredentialBase::IsCloudAssociationEnabled() {
  return GetGlobalFlagOrDefault(kRegCloudAssociation, 1);
}

// static
HRESULT CGaiaCredentialBase::OnDllRegisterServer() {
  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);

  if (!policy) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ScopedLsaPolicy::Create hr=" << putHR(hr);
    return hr;
  }

  PSID sid = nullptr;

  // Try to get existing username and password and then log on the user, if any
  // step fails, assume that a new user needs to be created.
  wchar_t gaia_username[kWindowsUsernameBufferLength];
  HRESULT hr = policy->RetrievePrivateData(kLsaKeyGaiaUsername, gaia_username,
                                           std::size(gaia_username));

  if (SUCCEEDED(hr)) {
    LOGFN(VERBOSE) << "Expecting gaia user '" << gaia_username << "' to exist.";
    wchar_t password[kWindowsPasswordBufferLength];
    hr = policy->RetrievePrivateData(kLsaKeyGaiaPassword, password,
                                     std::size(password));
    if (SUCCEEDED(hr)) {
      std::wstring local_domain = OSUserManager::GetLocalDomain();
      base::win::ScopedHandle token;
      hr = OSUserManager::Get()->CreateLogonToken(
          local_domain.c_str(), gaia_username, password,
          /*interactive=*/false, &token);
      if (SUCCEEDED(hr)) {
        hr = OSUserManager::Get()->GetUserSID(local_domain.c_str(),
                                              gaia_username, &sid);
        if (FAILED(hr)) {
          LOGFN(ERROR) << "GetUserSID(sid from existing user '" << gaia_username
                       << "') hr=" << putHR(hr);
          sid = nullptr;
        }
      }
    }
  }

  if (sid == nullptr) {
    hr = OSGaiaUserManager::Get()->CreateGaiaUser(&sid);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "OSGaiaUserManager::Get()->CreateGaiaUser hr="
                   << putHR(hr);
      return hr;
    }
  }

  if (!sid) {
    LOGFN(ERROR) << "No valid username could be found for the gaia user.";
    return HRESULT_FROM_WIN32(NERR_UserExists);
  }

  // Add "logon as batch" right.
  std::vector<std::wstring> rights{SE_BATCH_LOGON_NAME};
  hr = policy->AddAccountRights(sid, rights);
  ::LocalFree(sid);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "policy.AddAccountRights hr=" << putHR(hr);
    return hr;
  }
  return S_OK;
}

// static
HRESULT CGaiaCredentialBase::OnDllUnregisterServer() {
  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
  if (policy) {
    wchar_t password[kWindowsPasswordBufferLength];

    HRESULT hr = policy->RetrievePrivateData(kLsaKeyGaiaPassword, password,
                                             std::size(password));
    if (FAILED(hr))
      LOGFN(ERROR) << "policy.RetrievePrivateData hr=" << putHR(hr);

    hr = policy->RemovePrivateData(kLsaKeyGaiaPassword);
    if (FAILED(hr))
      LOGFN(ERROR) << "policy.RemovePrivateData hr=" << putHR(hr);

    OSUserManager* manager = OSUserManager::Get();
    PSID sid;

    wchar_t gaia_username[kWindowsUsernameBufferLength];
    hr = policy->RetrievePrivateData(kLsaKeyGaiaUsername, gaia_username,
                                     std::size(gaia_username));

    if (SUCCEEDED(hr)) {
      hr = policy->RemovePrivateData(kLsaKeyGaiaUsername);
      if (FAILED(hr)) {
        LOGFN(ERROR) << "RemovePrivateData GaiaUsername hr=" << putHR(hr);
      }
      hr = policy->RemovePrivateData(kLsaKeyGaiaSid);
      if (FAILED(hr)) {
        LOGFN(ERROR) << "RemovePrivateData kLsaKeyGaiaSid hr=" << putHR(hr);
      }

      std::wstring local_domain = OSUserManager::GetLocalDomain();

      hr = manager->GetUserSID(local_domain.c_str(), gaia_username, &sid);
      if (FAILED(hr)) {
        LOGFN(ERROR) << "manager.GetUserSID hr=" << putHR(hr);
        sid = nullptr;
      }

      hr = manager->RemoveUser(gaia_username, password);
      if (FAILED(hr))
        LOGFN(ERROR) << "manager->RemoveUser hr=" << putHR(hr);

      // Remove the account from LSA after the OS account is deleted.
      if (sid != nullptr) {
        hr = policy->RemoveAccount(sid);
        ::LocalFree(sid);
        if (FAILED(hr))
          LOGFN(ERROR) << "policy.RemoveAccount hr=" << putHR(hr);
      }
    } else {
      LOGFN(ERROR) << "Get gaia username failed hr=" << putHR(hr);
    }

  } else {
    LOGFN(ERROR) << "ScopedLsaPolicy::Create failed";
  }

  return S_OK;
}

CGaiaCredentialBase::CGaiaCredentialBase() {}

CGaiaCredentialBase::~CGaiaCredentialBase() {}

bool CGaiaCredentialBase::AreCredentialsValid() const {
  return CanAttemptWindowsLogon() &&
         IsWindowsPasswordValidForStoredUser(password_) == S_OK;
}

bool CGaiaCredentialBase::CanAttemptWindowsLogon() const {
  return username_.Length() > 0 && password_.Length() > 0;
}

HRESULT CGaiaCredentialBase::IsWindowsPasswordValidForStoredUser(
    BSTR password) const {
  if (username_.Length() == 0 || user_sid_.Length() == 0)
    return S_FALSE;

  if (::SysStringLen(password) == 0)
    return S_FALSE;
  OSUserManager* manager = OSUserManager::Get();
  return manager->IsWindowsPasswordValid(domain_, username_, password);
}

HRESULT CGaiaCredentialBase::GetStringValueImpl(DWORD field_id,
                                                wchar_t** value) {
  HRESULT hr = E_INVALIDARG;
  switch (field_id) {
    case FID_DESCRIPTION: {
      std::wstring description(
          GetStringResource(IDS_AUTH_FID_DESCRIPTION_BASE));
      hr = ::SHStrDupW(description.c_str(), value);
      break;
    }
    case FID_PROVIDER_LABEL: {
      std::wstring label(GetStringResource(IDS_AUTH_FID_PROVIDER_LABEL_BASE));
      hr = ::SHStrDupW(label.c_str(), value);
      break;
    }
    case FID_CURRENT_PASSWORD_FIELD: {
      hr = ::SHStrDupW(current_windows_password_.Length() > 0
                           ? current_windows_password_
                           : L"",
                       value);
      break;
    }
    case FID_FORGOT_PASSWORD_LINK: {
      std::wstring forgot_password(
          GetStringResource(IDS_FORGOT_PASSWORD_LINK_BASE));
      hr = ::SHStrDupW(forgot_password.c_str(), value);
      break;
    }
    default:
      break;
  }

  return hr;
}

HRESULT CGaiaCredentialBase::GetBitmapValueImpl(DWORD field_id,
                                                HBITMAP* phbmp) {
  HRESULT hr = E_INVALIDARG;
  switch (field_id) {
    case FID_PROVIDER_LOGO:
      *phbmp = ::LoadBitmap(CURRENT_MODULE(),
                            MAKEINTRESOURCE(IDB_GOOGLE_LOGO_SMALL));
      if (*phbmp)
        hr = S_OK;
      break;
    default:
      break;
  }

  return hr;
}

void CGaiaCredentialBase::ResetInternalState() {
  LOGFN(VERBOSE);
  username_.Empty();
  domain_.Empty();
  wait_for_report_result_ = false;

  SecurelyClearBuffer((BSTR)password_, password_.ByteLength());
  password_.Empty();

  current_windows_password_.Empty();

  SecurelyClearDictionaryValue(authentication_results_);
  needs_windows_password_ = false;
  request_force_password_change_ = false;
  result_status_ = STATUS_SUCCESS;

  TerminateLogonProcess();

  if (events_) {
    wchar_t* default_status_text = nullptr;
    GetStringValue(FID_DESCRIPTION, &default_status_text);
    events_->SetFieldString(this, FID_DESCRIPTION, default_status_text);
    events_->SetFieldState(this, FID_FORGOT_PASSWORD_LINK, CPFS_HIDDEN);
    events_->SetFieldState(this, FID_CURRENT_PASSWORD_FIELD, CPFS_HIDDEN);
    events_->SetFieldString(this, FID_CURRENT_PASSWORD_FIELD,
                            current_windows_password_);
    events_->SetFieldSubmitButton(this, FID_SUBMIT, FID_DESCRIPTION);
    UpdateSubmitButtonInteractiveState();
  }

  token_update_locker_.reset();
}

HRESULT CGaiaCredentialBase::GetBaseGlsCommandline(
    base::CommandLine* command_line) {
  DCHECK(command_line);

  base::FilePath gls_path = GetChromePath();

  if (gls_path.empty()) {
    LOGFN(ERROR) << "No path to chrome.exe could be found.";
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  command_line->SetProgram(gls_path);

  LOGFN(VERBOSE) << "App exe: " << command_line->GetProgram().value();

  command_line->AppendSwitch(kGcpwSigninSwitch);

  // Chrome allows specifying a group policy to run extensions on Windows
  // startup for all users. When GLS runs, the autostart extension is also
  // launched in the login screen. With --disable-extensions flag, this can be
  // prevented.
  command_line->AppendSwitch(switches::kDisableExtensions);

  // Get the language selected by the LanguageSelector and pass it onto Chrome.
  // The language will depend on if it is currently a SYSTEM logon (initial
  // logon) or a lock screen logon (from a user). If the user who locked the
  // screen has a specific language, that will be the one used for the UI
  // language.
  command_line->AppendSwitchNative("lang", GetSelectedLanguage());

  return S_OK;
}

HRESULT CGaiaCredentialBase::GetUserGlsCommandline(
    base::CommandLine* command_line) {
  return S_OK;
}

HRESULT CGaiaCredentialBase::GetGlsCommandline(
    base::CommandLine* command_line) {
  DCHECK(command_line);
  HRESULT hr = GetBaseGlsCommandline(command_line);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetBaseGlsCommandline hr=" << putHR(hr);
    return hr;
  }

  // If email domains are specified, only pass them to the GLS if the size is
  // exactly 1 so that it can be pre-populated in the sign-in screen.
  std::vector<std::wstring> email_domains_list = GetEmailDomainsList();
  if (GetEmailDomainsList().size() == 1)
    command_line->AppendSwitchNative(kEmailDomainsSwitch,
                                     email_domains_list[0]);

  hr = GetUserGlsCommandline(command_line);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetUserGlsCommandline hr=" << putHR(hr);
    return hr;
  }

  LOGFN(VERBOSE) << "Command line: " << command_line->GetCommandLineString();
  return S_OK;
}

void CGaiaCredentialBase::DisplayErrorInUI(LONG status,
                                           LONG substatus,
                                           BSTR status_text) {
  if (status != STATUS_SUCCESS) {
    if (events_)
      events_->SetFieldString(this, FID_DESCRIPTION, status_text);
  }
}

HRESULT CGaiaCredentialBase::HandleAutologon(
    CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* cpgsr,
    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* cpcs) {
  USES_CONVERSION;
  LOGFN(VERBOSE) << "user-sid=" << get_sid().m_str;
  DCHECK(cpgsr);
  DCHECK(cpcs);

  if (!CanAttemptWindowsLogon())
    return S_FALSE;

  bool password_updated = false;
  // If a password update is needed, check if the user entered their old
  // Windows password and it is valid. If it is, try to change the password
  // using the old password. If it isn't, return S_FALSE to state that the
  // login is not complete.
  if (needs_windows_password_) {
    OSUserManager* manager = OSUserManager::Get();
    if (request_force_password_change_) {
      HRESULT hr = manager->SetUserPassword(domain_, username_, password_);
      if (FAILED(hr)) {
        LOGFN(ERROR) << "SetUserPassword hr=" << putHR(hr);
        if (events_) {
          events_->SetFieldString(
              this, FID_DESCRIPTION,
              GetStringResource(IDS_FORCED_PASSWORD_CHANGE_FAILURE_BASE)
                  .c_str());
        }
        return S_FALSE;
      }
      password_updated = true;
    } else {
      HRESULT hr =
          IsWindowsPasswordValidForStoredUser(current_windows_password_);
      if (hr == S_OK) {
        hr = manager->ChangeUserPassword(domain_, username_,
                                         current_windows_password_, password_);

        if (FAILED(hr)) {
          if (hr != HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED)) {
            SetErrorMessageInPasswordField(hr);
            LOGFN(ERROR) << "ChangeUserPassword hr=" << putHR(hr);
            return hr;
          }
          LOGFN(ERROR) << "Access was denied to ChangeUserPassword.";
          password_ = current_windows_password_;
        } else {
          password_updated = true;
        }
      } else {
        if (current_windows_password_.Length() && events_) {
          UINT pasword_message_id = IDS_INVALID_PASSWORD_BASE;
          if (hr == HRESULT_FROM_WIN32(ERROR_ACCOUNT_LOCKED_OUT)) {
            pasword_message_id = IDS_ACCOUNT_LOCKED_BASE;
            LOGFN(ERROR) << "Account is locked.";
          }

          DisplayPasswordField(pasword_message_id);
        }
        return S_FALSE;
      }
    }
  }

  // Password was changed successfully, remove the old password information
  // so that a new password can be saved.
  if (password_updated) {
    HRESULT hr = PasswordRecoveryManager::Get()->ClearUserRecoveryPassword(
        OLE2CW(get_sid()));
    if (FAILED(hr))
      LOGFN(ERROR) << "ClearUserRecoveryPassword hr=" << putHR(hr);
  }

  // The OS user has already been created, so return all the information
  // needed to log them in.
  DWORD cpus = 0;
  provider()->GetUsageScenario(&cpus);
  HRESULT hr = BuildCredPackAuthenticationBuffer(
      domain_, get_username(), get_password(),
      static_cast<CREDENTIAL_PROVIDER_USAGE_SCENARIO>(cpus), cpcs);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "BuildCredPackAuthenticationBuffer hr=" << putHR(hr);
    return hr;
  }

  // Prevent update of token handle validity until after sign in has completed
  // so that a race condition doesn't end up locking out a user while they are
  // in the process of signing in. The lock must occur before restoring access
  // to the user below to prevent a race condition where the user would have
  // their access restored but then the token handle update thread is
  // immediately executed which causes the user to be locked again afterwards.
  PreventDenyAccessUpdate();

  // Restore user's access so that they can sign in.
  hr = AssociatedUserValidator::Get()->RestoreUserAccess(OLE2W(get_sid()));
  if (FAILED(hr) && hr != HRESULT_FROM_NT(STATUS_OBJECT_NAME_NOT_FOUND)) {
    LOGFN(ERROR) << "RestoreUserAccess hr=" << putHR(hr);
    ::CoTaskMemFree(cpcs->rgbSerialization);
    cpcs->rgbSerialization = nullptr;
    return hr;
  }

  cpcs->clsidCredentialProvider = CLSID_GaiaCredentialProvider;
  *cpgsr = CPGSR_RETURN_CREDENTIAL_FINISHED;

  return S_OK;
}

// Sets message ids corresponding to appropriate password change error response
// codes.
void CGaiaCredentialBase::SetErrorMessageInPasswordField(HRESULT hr) {
  UINT password_message_id;
  switch (hr) {
    case HRESULT_FROM_WIN32(ERROR_INVALID_PASSWORD):
      password_message_id = IDS_INVALID_PASSWORD_BASE;
      break;
    case HRESULT_FROM_WIN32(NERR_InvalidComputer):
      // This condition should never be invoked.
      password_message_id = IDS_INVALID_COMPUTER_NAME_ERROR_BASE;
      break;
    case HRESULT_FROM_WIN32(NERR_NotPrimary):
      password_message_id = IDS_AD_PASSWORD_CHANGE_DENIED_BASE;
      break;
    case HRESULT_FROM_WIN32(NERR_UserNotFound):
      // This condition should never be invoked.
      password_message_id = IDS_USER_NOT_FOUND_PASSWORD_ERROR_BASE;
      break;
    case HRESULT_FROM_WIN32(NERR_PasswordTooShort):
      password_message_id = IDS_PASSWORD_COMPLEXITY_ERROR_BASE;
      break;
    default:
      // This condition should never be invoked.
      password_message_id = IDS_UNKNOWN_PASSWORD_ERROR_BASE;
      break;
  }
  DisplayPasswordField(password_message_id);
}

bool CGaiaCredentialBase::BlockingPasswordError(UINT message_id) {
  for (auto e : kPasswordErrors) {
    if (e == message_id)
      return true;
  }
  return false;
}

// static
void CGaiaCredentialBase::TellOmahaDidRun() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Tell omaha that product was used.  Best effort only.
  //
  // This code always runs as LocalSystem, which means that HKCU maps to
  // HKU\.Default.  This is OK because omaha reads the "dr" value from subkeys
  // of HKEY_USERS.
  base::win::RegKey key;
  LONG sts = key.Create(HKEY_CURRENT_USER, kRegUpdaterClientStateAppPath,
                        KEY_SET_VALUE | KEY_WOW64_32KEY);
  if (sts != ERROR_SUCCESS) {
    LOGFN(VERBOSE) << "Unable to open omaha key sts=" << sts;
  } else {
    sts = key.WriteValue(L"dr", L"1");
    if (sts != ERROR_SUCCESS)
      LOGFN(WARNING) << "Unable to write omaha dr value sts=" << sts;
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

void CGaiaCredentialBase::PreventDenyAccessUpdate() {
  if (!token_update_locker_) {
    token_update_locker_ =
        std::make_unique<AssociatedUserValidator::ScopedBlockDenyAccessUpdate>(
            AssociatedUserValidator::Get());
  }
}

// static
BSTR CGaiaCredentialBase::AllocErrorString(UINT id) {
  CComBSTR str(GetStringResource(id).c_str());
  return str.Detach();
}

// static
BSTR CGaiaCredentialBase::AllocErrorString(
    UINT id,
    const std::vector<std::wstring>& replacements) {
  CComBSTR str(GetStringResource(id, replacements).c_str());
  return str.Detach();
}

// static
HRESULT CGaiaCredentialBase::GetInstallDirectory(base::FilePath* path) {
  DCHECK(path);

  if (!base::PathService::Get(base::FILE_MODULE, path)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "Get(FILE_MODULE) hr=" << putHR(hr);
    return hr;
  }

  *path = path->DirName();
  return S_OK;
}

// ICredentialProviderCredential //////////////////////////////////////////////

HRESULT CGaiaCredentialBase::Advise(ICredentialProviderCredentialEvents* cpce) {
  LOGFN(VERBOSE);
  events_ = cpce;
  return S_OK;
}

HRESULT CGaiaCredentialBase::UnAdvise(void) {
  LOGFN(VERBOSE);
  events_.Reset();

  return S_OK;
}

HRESULT CGaiaCredentialBase::SetSelected(BOOL* auto_login) {
  *auto_login = CanAttemptWindowsLogon();
  LOGFN(VERBOSE) << "auto-login=" << *auto_login;

  // After this point the user is able to interact with the winlogon and thus
  // can avoid potential crash loops so the startup sentinel can be deleted.
  DeleteStartupSentinel();
  return S_OK;
}

HRESULT CGaiaCredentialBase::SetDeselected(void) {
  LOGFN(VERBOSE);

  // This check is trying to handle the scenario when GetSerialization finishes
  // with cpgsr set as CPGSR_RETURN_CREDENTIAL_FINISHED which indicates that
  // the windows autologon is ready to go. In this case ideally ReportResult
  // should be invoked by the windows login UI process prior to SetDeselected.
  // But for OtherUserCredential scenario, SetDeselected is being invoked
  // prior to ReportResult which is leading to clearing of the internalstate
  // prior to saving the account user info in ReportResult.
  if (!wait_for_report_result_) {
    // Cancel logon so that the next time this credential is clicked everything
    // has to be re-entered by the user. This prevents a Windows password
    // entered into the password field by the user from being persisted too
    // long. The behaviour is similar to that of the normal windows password
    // text box. Whenever a different user is selected and then the original
    // credential is selected again, the password is cleared.
    ResetInternalState();
  }
  return S_OK;
}

HRESULT CGaiaCredentialBase::GetFieldState(
    DWORD field_id,
    CREDENTIAL_PROVIDER_FIELD_STATE* pcpfs,
    CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* pcpfis) {
  HRESULT hr = E_INVALIDARG;
  switch (field_id) {
    case FID_DESCRIPTION:
    case FID_SUBMIT:
      *pcpfs = CPFS_DISPLAY_IN_SELECTED_TILE;
      *pcpfis = CPFIS_NONE;
      hr = S_OK;
      break;
    case FID_PROVIDER_LOGO:
      *pcpfs = ::IsWindows8OrGreater() ? CPFS_HIDDEN : CPFS_DISPLAY_IN_BOTH;
      *pcpfis = CPFIS_NONE;
      hr = S_OK;
      break;
    case FID_PROVIDER_LABEL:
      *pcpfs = ::IsWindows8OrGreater() ? CPFS_HIDDEN
                                       : CPFS_DISPLAY_IN_DESELECTED_TILE;
      *pcpfis = CPFIS_NONE;
      hr = S_OK;
      break;
    case FID_CURRENT_PASSWORD_FIELD:
      *pcpfs = CPFS_HIDDEN;
      *pcpfis = CPFIS_NONE;
      hr = S_OK;
      break;
    case FID_FORGOT_PASSWORD_LINK:
      *pcpfs = CPFS_HIDDEN;
      *pcpfis = CPFIS_NONE;
      hr = S_OK;
      break;
    default:
      break;
  }
  LOGFN(VERBOSE) << "hr=" << putHR(hr) << " field=" << field_id
                 << " state=" << *pcpfs << " inter-state=" << *pcpfis;
  return hr;
}

HRESULT CGaiaCredentialBase::GetStringValue(DWORD field_id, wchar_t** value) {
  return GetStringValueImpl(field_id, value);
}

HRESULT CGaiaCredentialBase::GetBitmapValue(DWORD field_id, HBITMAP* phbmp) {
  return GetBitmapValueImpl(field_id, phbmp);
}

HRESULT CGaiaCredentialBase::GetCheckboxValue(DWORD field_id,
                                              BOOL* pbChecked,
                                              wchar_t** ppszLabel) {
  // No checkboxes.
  return E_NOTIMPL;
}

HRESULT CGaiaCredentialBase::GetSubmitButtonValue(DWORD field_id,
                                                  DWORD* adjacent_to) {
  HRESULT hr = E_INVALIDARG;
  switch (field_id) {
    case FID_SUBMIT:
      *adjacent_to = FID_DESCRIPTION;
      hr = S_OK;
      break;
    default:
      break;
  }

  return hr;
}

HRESULT CGaiaCredentialBase::GetComboBoxValueCount(DWORD field_id,
                                                   DWORD* pcItems,
                                                   DWORD* pdwSelectedItem) {
  // No comboboxes.
  return E_NOTIMPL;
}

HRESULT CGaiaCredentialBase::GetComboBoxValueAt(DWORD field_id,
                                                DWORD dwItem,
                                                wchar_t** ppszItem) {
  // No comboboxes.
  return E_NOTIMPL;
}

HRESULT CGaiaCredentialBase::SetStringValue(DWORD field_id,
                                            const wchar_t* psz) {
  USES_CONVERSION;

  HRESULT hr = E_INVALIDARG;
  switch (field_id) {
    case FID_CURRENT_PASSWORD_FIELD:
      if (needs_windows_password_) {
        current_windows_password_ = W2COLE(psz);
        UpdateSubmitButtonInteractiveState();
      }
      hr = S_OK;
      break;
  }
  return hr;
}

HRESULT CGaiaCredentialBase::SetCheckboxValue(DWORD field_id, BOOL bChecked) {
  // No checkboxes.
  return E_NOTIMPL;
}

HRESULT CGaiaCredentialBase::SetComboBoxSelectedValue(DWORD field_id,
                                                      DWORD dwSelectedItem) {
  // No comboboxes.
  return E_NOTIMPL;
}

bool CGaiaCredentialBase::CanProceedToLogonStub(wchar_t** status_text) {
  bool can_proceed_to_logon_stub = true;
  BSTR error_message;

  // Restricted domains key must be set to proceed with logon stub.
  std::wstring restricted_domains = GetEmailDomains();
  if (restricted_domains.empty()) {
    can_proceed_to_logon_stub = false;
    error_message = AllocErrorString(IDS_EMAIL_MISMATCH_BASE);
    LOGFN(ERROR) << "Restricted domains registry key must be set";
  } else if (!InternetAvailabilityChecker::Get()->HasInternetConnection()) {
    // If there is no internet connection, just abort right away.
    can_proceed_to_logon_stub = false;
    error_message = AllocErrorString(IDS_NO_NETWORK_BASE);
    LOGFN(VERBOSE) << "No internet connection";
  }

  if (!can_proceed_to_logon_stub) {
    ::SHStrDupW(OLE2CW(error_message), status_text);
    ::SysFreeString(error_message);
  }

  return can_proceed_to_logon_stub;
}

HRESULT CGaiaCredentialBase::CommandLinkClicked(DWORD dwFieldID) {
  if (dwFieldID == FID_FORGOT_PASSWORD_LINK && needs_windows_password_) {
    request_force_password_change_ = !request_force_password_change_;
    DisplayPasswordField(IDS_PASSWORD_UPDATE_NEEDED_BASE);
    UpdateSubmitButtonInteractiveState();
    return S_OK;
  }

  return E_INVALIDARG;
}

HRESULT CGaiaCredentialBase::GetSerialization(
    CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* cpgsr,
    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* cpcs,
    wchar_t** status_text,
    CREDENTIAL_PROVIDER_STATUS_ICON* status_icon) {
  USES_CONVERSION;
  LOGFN(VERBOSE);
  DCHECK(status_text);
  DCHECK(status_icon);

  *status_text = nullptr;
  *status_icon = CPSI_NONE;
  memset(cpcs, 0, sizeof(*cpcs));

  // This may be a long running function so disable user input while processing.
  if (events_) {
    events_->SetFieldInteractiveState(this, FID_SUBMIT, CPFIS_DISABLED);
    events_->SetFieldInteractiveState(this, FID_CURRENT_PASSWORD_FIELD,
                                      CPFIS_DISABLED);
  }

  HRESULT hr = HandleAutologon(cpgsr, cpcs);

  bool submit_button_enabled = false;
  // Don't clear the state of the credential on error. The error can occur
  // because the user is locked out or entered an incorrect old password when
  // trying to update their password. In these situations it may still be
  // possible to sign in with the information that is currently available if
  // the problem can be fixed externally so keep all the information for now.
  if (FAILED(hr)) {
    LOGFN(ERROR) << "HandleAutologon hr=" << putHR(hr);
    *status_icon = CPSI_ERROR;
    *cpgsr = CPGSR_RETURN_NO_CREDENTIAL_FINISHED;
  } else if (hr == S_FALSE) {
    // If HandleAutologon returns S_FALSE, then there was not enough information
    // to log the user on or they need to update their password and gave an
    // invalid old password.  Display the Gaia sign in page if there is not
    // sufficient Gaia credentials or just return
    // CPGSR_NO_CREDENTIAL_NOT_FINISHED to wait for the user to try a new
    // password.

    // Logon process is still running or windows password needs to be entered,
    // return that serialization is not finished so that a second logon stub
    // isn't started.
    if (logon_ui_process_ != INVALID_HANDLE_VALUE || needs_windows_password_) {
      *cpgsr = CPGSR_NO_CREDENTIAL_NOT_FINISHED;

      // Warn that password needs update.
      if (needs_windows_password_)
        *status_icon = CPSI_WARNING;

      hr = S_OK;
    } else {
      LOGFN(VERBOSE) << "HandleAutologon hr=" << putHR(hr);
      TellOmahaDidRun();

      if (!CanProceedToLogonStub(status_text)) {
        *status_icon = CPSI_NONE;
        *cpgsr = CPGSR_NO_CREDENTIAL_FINISHED;
        submit_button_enabled = UpdateSubmitButtonInteractiveState();

        hr = S_OK;
      } else {
        // The account creation is async so we are not done yet.
        *cpgsr = CPGSR_NO_CREDENTIAL_NOT_FINISHED;

        // The expectation is that the UI will eventually return the username,
        // password, and auth to this CGaiaCredentialBase object, so that
        // OnUserAuthenticated() can be called, followed by
        // provider_->OnUserAuthenticated().
        hr = CreateAndRunLogonStub();
        if (FAILED(hr)) {
          std::wstring error_message(
              GetStringResource(IDS_FAILED_CREATE_LOGON_STUB_BASE));
          ::SHStrDupW(OLE2CW(error_message.c_str()), status_text);

          *status_icon = CPSI_NONE;
          *cpgsr = CPGSR_NO_CREDENTIAL_FINISHED;
          submit_button_enabled = UpdateSubmitButtonInteractiveState();

          hr = S_OK;
        }
      }
    }
  } else {
    *status_icon = CPSI_SUCCESS;
  }

  // Logon is not complete, re-enable UI as needed.
  if (*cpgsr != CPGSR_NO_CREDENTIAL_FINISHED &&
      *cpgsr != CPGSR_RETURN_CREDENTIAL_FINISHED &&
      *cpgsr != CPGSR_RETURN_NO_CREDENTIAL_FINISHED) {
    if (events_) {
      events_->SetFieldInteractiveState(
          this, FID_CURRENT_PASSWORD_FIELD,
          needs_windows_password_ ? CPFIS_FOCUSED : CPFIS_NONE);
    }
    submit_button_enabled = UpdateSubmitButtonInteractiveState();
  }

  // If user interaction is enabled that means we are not trying to do final
  // sign in of the account so we can re-enable token updates.
  if (submit_button_enabled)
    token_update_locker_.reset();

  // If cpgsr is CPGSR_RETURN_CREDENTIAL_FINISHED and the status is S_OK, then
  // report result would be invoked. So we shouldn't be resetting the internal
  // state prior to report result getting triggered.
  if (*cpgsr == CPGSR_RETURN_CREDENTIAL_FINISHED && hr == S_OK) {
    wait_for_report_result_ = true;
  }

  // Otherwise, keep the ui disabled forever now. ReportResult will eventually
  // be called on success or failure and the reset of the state of the
  // credential will be done there.
  return hr;
}

HRESULT CGaiaCredentialBase::CreateAndRunLogonStub() {
  LOGFN(VERBOSE);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  HRESULT hr = GetGlsCommandline(&command_line);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetGlsCommandline hr=" << putHR(hr);
    return hr;
  }

  // The process should start on the interactive window station (since it
  // needs to show a UI) but on its own desktop so that it cannot interact
  // with winlogon on user windows.
  std::unique_ptr<UIProcessInfo> uiprocinfo(new UIProcessInfo);
  PSID logon_sid;
  hr = CreateGaiaLogonToken(&uiprocinfo->logon_token, &logon_sid);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "CreateGaiaLogonToken hr=" << putHR(hr);
    return hr;
  }

  OSProcessManager* process_manager = OSProcessManager::Get();
  hr = process_manager->SetupPermissionsForLogonSid(logon_sid);
  LocalFree(logon_sid);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "SetupPermissionsForLogonSid hr=" << putHR(hr);
    return hr;
  }

  hr = ForkGaiaLogonStub(process_manager, command_line, uiprocinfo.get());
  if (FAILED(hr)) {
    LOGFN(ERROR) << "ForkGaiaLogonStub hr=" << putHR(hr);
    return hr;
  }

  // Save the handle to the logon UI process so that it can be killed should
  // the credential be Unadvise()d.
  DCHECK_EQ(logon_ui_process_, INVALID_HANDLE_VALUE);
  logon_ui_process_ = uiprocinfo->procinfo.process_handle();

  uiprocinfo->credential = this;

  // Background thread takes ownership of |uiprocinfo|.
  unsigned int wait_thread_id;
  UIProcessInfo* puiprocinfo = uiprocinfo.release();
  uintptr_t wait_thread = _beginthreadex(nullptr, 0, WaitForLoginUI,
                                         puiprocinfo, 0, &wait_thread_id);
  if (wait_thread != 0) {
    LOGFN(VERBOSE) << "Started wait thread id=" << wait_thread_id;
    ::CloseHandle(reinterpret_cast<HANDLE>(wait_thread));
  } else {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "Unable to start wait thread hr=" << putHR(hr);
    ::TerminateProcess(puiprocinfo->procinfo.process_handle(), kUiecKilled);
    delete puiprocinfo;
    return hr;
  }

  // This function returns success, which means that GetSerialization() will
  // return success.  CGaiaCredentialBase is now committed to telling
  // CGaiaCredentialProvider whether the serialization eventually succeeds or
  // fails, so that CGaiaCredentialProvider can in turn inform winlogon about
  // what happened.
  LOGFN(VERBOSE) << "cleaning up";
  return S_OK;
}

// static
HRESULT CGaiaCredentialBase::CreateGaiaLogonToken(
    base::win::ScopedHandle* token,
    PSID* sid) {
  DCHECK(token);
  DCHECK(sid);

  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
  if (!policy) {
    LOGFN(ERROR) << "LsaOpenPolicy failed";
    return E_UNEXPECTED;
  }

  wchar_t gaia_username[kWindowsUsernameBufferLength];
  HRESULT hr = policy->RetrievePrivateData(kLsaKeyGaiaUsername, gaia_username,
                                           std::size(gaia_username));

  if (FAILED(hr)) {
    LOGFN(ERROR) << "Retrieve gaia username hr=" << putHR(hr);
    return hr;
  }
  wchar_t password[kWindowsPasswordBufferLength];
  hr = policy->RetrievePrivateData(kLsaKeyGaiaPassword, password,
                                   std::size(password));
  if (FAILED(hr)) {
    LOGFN(ERROR) << "Retrieve password for gaia user '" << gaia_username
                 << "' hr=" << putHR(hr);
    return hr;
  }

  std::wstring local_domain = OSUserManager::GetLocalDomain();
  hr = OSUserManager::Get()->CreateLogonToken(local_domain.c_str(),
                                              gaia_username, password,
                                              /*interactive=*/false, token);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "CreateLogonToken hr=" << putHR(hr);
    return hr;
  }

  hr = OSProcessManager::Get()->GetTokenLogonSID(*token, sid);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetTokenLogonSID hr=" << putHR(hr);
    token->Close();
    return hr;
  }

  wchar_t* sid_string;
  if (::ConvertSidToStringSid(*sid, &sid_string)) {
    LOGFN(VERBOSE) << "logon-sid=" << sid_string;
    LocalFree(sid_string);
  } else {
    LOGFN(ERROR) << "logon-sid=<can't get string>";
  }

  return S_OK;
}

// static
HRESULT CGaiaCredentialBase::ForkGaiaLogonStub(
    OSProcessManager* process_manager,
    const base::CommandLine& command_line,
    UIProcessInfo* uiprocinfo) {
  LOGFN(VERBOSE);
  DCHECK(process_manager);
  DCHECK(uiprocinfo);

  ScopedStartupInfo startupinfo(kDesktopFullName);

  // Only create a stdout pipe for the logon stub process. On some machines
  // Chrome will not startup properly when also given a stderror pipe due
  // to access restrictions. For the purposes of the credential provider
  // only the output of stdout matters.
  HRESULT hr =
      InitializeStdHandles(CommDirection::kChildToParentOnly, kStdOutput,
                           &startupinfo, &uiprocinfo->parent_handles);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "InitializeStdHandles hr=" << putHR(hr);
    return hr;
  }

  // The process is created suspended so that we can adjust its environment
  // before it starts.  Also, it must not run before it is added to the job
  // object.
  hr = process_manager->CreateProcessWithToken(
      uiprocinfo->logon_token, command_line, startupinfo.GetInfo(),
      &uiprocinfo->procinfo);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "process_manager->CreateProcessWithToken hr=" << putHR(hr);
    return hr;
  }

  LOGFN(VERBOSE) << "pid=" << uiprocinfo->procinfo.process_id()
                 << " tid=" << uiprocinfo->procinfo.thread_id();

  // Don't create a job here with UI restrictions, since win10 does not allow
  // nested jobs unless all jobs don't specify UI restrictions.  Since chrome
  // will set a job with UI restrictions for renderer/gpu/etc processes, setting
  // one here causes chrome to fail.

  // Environment is fully set up for UI, so let it go.
  if (::ResumeThread(uiprocinfo->procinfo.thread_handle()) ==
      static_cast<DWORD>(-1)) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ResumeThread hr=" << putHR(hr);
    ::TerminateProcess(uiprocinfo->procinfo.process_handle(), kUiecKilled);
    return hr;
  }

  // Don't close the desktop until after the process has started and acquired
  // a handle to it.  Otherwise, the desktop will be destroyed and the process
  // will fail to start.
  //
  // WaitForInputIdle() return immediately with an error if the process
  // created is a console app.  In production this will not be the case,
  // however in tests this may happen.  However, tests are not concerned with
  // the destruction of the desktop since one is not created.
  DWORD ret = ::WaitForInputIdle(uiprocinfo->procinfo.process_handle(), 10000);
  if (ret != 0)
    LOGFN(VERBOSE) << "WaitForInputIdle, ret=" << ret;

  return S_OK;
}

HRESULT CGaiaCredentialBase::ForkPerformPostSigninActionsStub(
    const base::Value::Dict& dict,
    BSTR* status_text) {
  LOGFN(VERBOSE);
  DCHECK(status_text);

  ScopedStartupInfo startupinfo;
  StdParentHandles parent_handles;
  HRESULT hr =
      InitializeStdHandles(CommDirection::kParentToChildOnly, kAllStdHandles,
                           &startupinfo, &parent_handles);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "InitializeStdHandles hr=" << putHR(hr);
    *status_text = AllocErrorString(IDS_INTERNAL_ERROR_BASE);
    return hr;
  }

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  hr = GetCommandLineForEntrypoint(CURRENT_MODULE(),
                                   L"PerformPostSigninActions", &command_line);
  if (hr == S_FALSE) {
    // This happens in tests.  It means this code is running inside the
    // unittest exe and not the credential provider dll.  Just ignore saving
    // the account info.
    LOGFN(VERBOSE) << "Not running SAIS";
    return S_OK;
  } else if (FAILED(hr)) {
    LOGFN(ERROR) << "GetCommandLineForEntryPoint hr=" << putHR(hr);
    *status_text = AllocErrorString(IDS_INTERNAL_ERROR_BASE);
    return hr;
  }

  // Mark this process as a child process so that it doesn't try to
  // start a crashpad handler process. Only the main entry point
  // into the dll should start the handler process.
  command_line.AppendSwitchASCII(switches::kProcessType,
                                 "gcpw-save-account-info");

  base::win::ScopedProcessInformation procinfo;
  hr = OSProcessManager::Get()->CreateRunningProcess(
      command_line, startupinfo.GetInfo(), &procinfo);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "OSProcessManager::CreateRunningProcess hr=" << putHR(hr);
    *status_text = AllocErrorString(IDS_INTERNAL_ERROR_BASE);
    return hr;
  }

  // Write account info to stdin of child process.  This buffer is read by
  // PerformPostSigninActionsW() in dllmain.cpp.  If this fails, chrome won't
  // pick up the credentials from the credential provider and will need to sign
  // in manually.
  std::string json;
  if (base::JSONWriter::Write(dict, &json)) {
    const DWORD buffer_size = json.length() + 1;
    LOGFN(VERBOSE) << "Json size: " << buffer_size;

    DWORD written = 0;
    // First, write the buffer size then write the buffer content.
    if (!::WriteFile(parent_handles.hstdin_write.Get(), &buffer_size,
                     sizeof(buffer_size), &written, /*lpOverlapped=*/nullptr)) {
      HRESULT hrWrite = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "WriteFile hr=" << putHR(hrWrite);
    } else if (!::WriteFile(parent_handles.hstdin_write.Get(), json.c_str(),
                            buffer_size, &written, /*lpOverlapped=*/nullptr)) {
      HRESULT hrWrite = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "WriteFile hr=" << putHR(hrWrite);
    }
    SecurelyClearString(json);
  } else {
    LOGFN(ERROR) << "base::JSONWriter::Write failed";
  }

  return S_OK;
}

// static
unsigned __stdcall CGaiaCredentialBase::WaitForLoginUI(void* param) {
  USES_CONVERSION;
  DCHECK(param);
  std::unique_ptr<UIProcessInfo> uiprocinfo(
      reinterpret_cast<UIProcessInfo*>(param));

  // Make sure COM is initialized in this thread. This thread must be
  // initialized as an MTA or the call to enroll with MDM causes a crash in COM.
  base::win::ScopedCOMInitializer com_initializer(
      base::win::ScopedCOMInitializer::kMTA);
  if (!com_initializer.Succeeded()) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ScopedCOMInitializer failed hr=" << putHR(hr);
    return hr;
  }

  CComBSTR status_text;
  DWORD exit_code;
  std::string json_result;
  HRESULT hr = WaitForLoginUIAndGetResult(uiprocinfo.get(), &json_result,
                                          &exit_code, &status_text);
  if (SUCCEEDED(hr)) {
    // Notify that the new user is created.
    // TODO(rogerta): Docs say this should not be called on a background
    // thread, but on the thread that received the
    // CGaiaCredentialBase::Advise() call. Seems to work for now though, but I
    // suspect there could be a problem if this call races with a call to
    // CGaiaCredentialBase::Unadvise().
    std::wstring json_result16 = base::UTF8ToWide(json_result);
    CComBSTR result_string(W2COLE(json_result16.c_str()));
    SecurelyClearString(json_result16);

    hr = uiprocinfo->credential->OnUserAuthenticated(result_string,
                                                     &status_text);
    SecurelyClearBuffer((BSTR)result_string, result_string.ByteLength());
  }

  SecurelyClearString(json_result);

  // If the process was killed by the credential in Terminate(), don't process
  // the error message since it is possible that the credential and/or the
  // provider no longer exists.
  if (FAILED(hr)) {
    if (hr != E_ABORT)
      LOGFN(ERROR) << "WaitForLoginUIAndGetResult hr=" << putHR(hr);

    // If hr is E_ABORT, this is a user initiated cancel.  Don't consider this
    // an error.
    LONG sts = hr == E_ABORT ? STATUS_SUCCESS : HRESULT_CODE(hr);

    // Either WaitForLoginUIAndGetResult did not fail or there should be an
    // error message to display.
    DCHECK(sts == STATUS_SUCCESS || status_text != nullptr);
    hr = uiprocinfo->credential->ReportError(sts, STATUS_SUCCESS, status_text);
    if (FAILED(hr))
      LOGFN(ERROR) << "uiprocinfo->credential->ReportError hr=" << putHR(hr);
  }

  LOGFN(VERBOSE) << "done";
  return 0;
}

// static
HRESULT CGaiaCredentialBase::PerformActions(
    const base::Value::Dict& properties) {
  LOGFN(VERBOSE);

  std::wstring sid = GetDictString(properties, kKeySID);
  if (sid.empty()) {
    LOGFN(ERROR) << "SID is empty";
    return E_INVALIDARG;
  }

  std::wstring username = GetDictString(properties, kKeyUsername);
  if (username.empty()) {
    LOGFN(ERROR) << "Username is empty";
    return E_INVALIDARG;
  }

  std::wstring password = GetDictString(properties, kKeyPassword);
  if (password.empty()) {
    LOGFN(ERROR) << "Password is empty";
    return E_INVALIDARG;
  }

  std::wstring domain = GetDictString(properties, kKeyDomain);

  // Load the user's profile so that their registry hive is available.
  auto profile = ScopedUserProfile::Create(sid, domain, username, password);

  if (!profile) {
    LOGFN(ERROR) << "Could not load user profile";
    return E_UNEXPECTED;
  }

  HRESULT hr = profile->SaveAccountInfo(properties);
  if (FAILED(hr))
    LOGFN(ERROR) << "profile.SaveAccountInfo failed (cont) hr=" << putHR(hr);

  // TODO(crbug.com/41466886): Use the down scoped kKeyMdmAccessToken instead
  // of login scoped token.
  std::string access_token = GetDictStringUTF8(properties, kKeyAccessToken);
  if (access_token.empty()) {
    LOGFN(ERROR) << "Access token is empty.";
    return E_FAIL;
  }
  // Update the password recovery information if possible.
  hr = PasswordRecoveryManager::Get()->StoreWindowsPasswordIfNeeded(
      sid, access_token, password);
  SecurelyClearString(password);
  if (FAILED(hr) && hr != E_NOTIMPL)
    LOGFN(ERROR) << "StoreWindowsPasswordIfNeeded hr=" << putHR(hr);

  hr = GenerateGCPWDmToken(sid);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GenerateGCPWDmToken hr=" << putHR(hr);
  }

  // Upload device details to gem database.
  hr = GemDeviceDetailsManager::Get()->UploadDeviceDetails(access_token, sid,
                                                           username, domain);

  DWORD device_upload_failures = 0;
  GetUserProperty(sid, kRegDeviceDetailsUploadFailures,
                  &device_upload_failures);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "UploadDeviceDetails hr=" << putHR(hr);
    ++device_upload_failures;
  } else {
    device_upload_failures = 0;
  }
  SetUserProperty(sid, kRegDeviceDetailsUploadStatus, SUCCEEDED(hr) ? 1 : 0);
  SetUserProperty(sid, kRegDeviceDetailsUploadFailures, device_upload_failures);

  // Below setter is only used for unit testing.
  GemDeviceDetailsManager::Get()->SetUploadStatusForTesting(hr);

  return hr;
}

// static
HRESULT CGaiaCredentialBase::PerformPostSigninActions(
    const base::Value::Dict& properties,
    bool com_initialized) {
  LOGFN(VERBOSE);
  HRESULT hr = S_OK;

  if (com_initialized) {
    hr = credential_provider::CGaiaCredentialBase::PerformActions(properties);
    if (FAILED(hr))
      LOGFN(ERROR) << "PerformActions hr=" << putHR(hr);

    // Try to enroll the machine to MDM here. MDM requires a user to be signed
    // on to an interactive session to succeed and when we call this function
    // the user should have been successfully signed on at that point and able
    // to finish the enrollment.
    hr = credential_provider::EnrollToGoogleMdmIfNeeded(properties);
    if (FAILED(hr))
      LOGFN(ERROR) << "EnrollToGoogleMdmIfNeeded hr=" << putHR(hr);
  }

  // Ensure GCPW gets updated to the correct version.
  if (DevicePoliciesManager::Get()->CloudPoliciesEnabled()) {
    DevicePoliciesManager::Get()->EnforceGcpwUpdatePolicy();
  }

  // TODO(crbug.com/41466886): Use the down scoped kKeyMdmAccessToken instead
  // of login scoped token.
  std::string access_token = GetDictStringUTF8(properties, kKeyAccessToken);

  // Finally upload event logs to cloud storage.
  if (!access_token.empty()) {
    hr = EventLogsUploadManager::Get()->UploadEventViewerLogs(access_token);
    if (FAILED(hr) && hr != E_NOTIMPL)
      LOGFN(ERROR) << "UploadEventViewerLogs hr=" << putHR(hr);
  } else {
    LOGFN(ERROR) << "Access token is empty. Cannot upload logs.";
  }

  return hr;
}

// Registers OS user - gaia user association in HKEY_LOCAL_MACHINE registry
// hive.
HRESULT
RegisterAssociation(const std::wstring& sid,
                    const std::wstring& id,
                    const std::wstring& email,
                    const std::wstring& domain,
                    const std::wstring& username,
                    const std::wstring& token_handle) {
  // Save token handle.  This handle will be used later to determine if the
  // the user has changed their password since the account was created.
  HRESULT hr = SetUserProperty(sid, kUserTokenHandle, token_handle);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "SetUserProperty(th) hr=" << putHR(hr);
    return hr;
  }

  hr = SetUserProperty(sid, kUserId, id);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "SetUserProperty(id) hr=" << putHR(hr);
    return hr;
  }

  hr = SetUserProperty(sid, kUserEmail, email);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "SetUserProperty(email) hr=" << putHR(hr);
    return hr;
  }

  hr = SetUserProperty(sid, base::UTF8ToWide(kKeyDomain), domain);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "SetUserProperty(domain) hr=" << putHR(hr);
    return hr;
  }

  hr = SetUserProperty(sid, base::UTF8ToWide(kKeyUsername), username);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "SetUserProperty(user) hr=" << putHR(hr);
    return hr;
  }

  if (IsGemEnabled()) {
    hr = SetUserProperty(sid, kKeyAcceptTos, 1u);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "SetUserProperty(acceptTos) hr=" << putHR(hr);
      return hr;
    }
  }

  return S_OK;
}

HRESULT CGaiaCredentialBase::ReportResult(
    NTSTATUS status,
    NTSTATUS substatus,
    wchar_t** ppszOptionalStatusText,
    CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon) {
  LOGFN(VERBOSE) << "status=" << putHR(status)
                 << " substatus=" << putHR(substatus);

  if (status == STATUS_SUCCESS && authentication_results_) {
    // Update the sid, domain, username and password in
    // |authentication_results_| with the real Windows information for the user
    // so that the PerformPostSigninActions process can correctly sign in to the
    // user account.
    authentication_results_->Set(
        kKeySID, base::Value(base::WideToUTF8((BSTR)user_sid_)));
    authentication_results_->Set(kKeyDomain,
                                 base::Value(base::WideToUTF8((BSTR)domain_)));
    authentication_results_->Set(
        kKeyUsername, base::Value(base::WideToUTF8((BSTR)username_)));
    authentication_results_->Set(
        kKeyPassword, base::Value(base::WideToUTF8((BSTR)password_)));

    std::wstring gaia_id = GetDictString(*authentication_results_, kKeyId);
    if (gaia_id.empty()) {
      LOGFN(ERROR) << "Id is empty";
      return E_INVALIDARG;
    }

    std::wstring email = GetDictString(*authentication_results_, kKeyEmail);
    if (email.empty()) {
      LOGFN(ERROR) << "Email is empty";
      return E_INVALIDARG;
    }

    // Os user - gaia user association is saved in HKEY_LOCAL_MACHINE. So, we
    // can attempt saving association even before calling forked process. Forked
    // process will also re-write everything saved here as well as valid token
    // handle. Token handle is saved as empty here, so that if for any reason
    // forked process fails to save association, it will enforce re-auth due to
    // invalid token handle.
    std::wstring sid = OLE2CW(user_sid_);
    HRESULT hr = RegisterAssociation(sid, gaia_id, email, (BSTR)domain_,
                                     (BSTR)username_, /*token_handle*/ L"");
    if (FAILED(hr))
      return hr;

    // At this point the user and password stored in authentication_results_
    // should match what is stored in username_ and password_ so the
    // PerformPostSigninActions process can be forked.
    CComBSTR status_text;
    hr = ForkPerformPostSigninActionsStub(*authentication_results_,
                                          &status_text);
    if (FAILED(hr))
      LOGFN(ERROR) << "ForkPerformPostSigninActionsStub hr=" << putHR(hr);
  }

  *ppszOptionalStatusText = nullptr;
  *pcpsiOptionalStatusIcon = CPSI_NONE;
  ResetInternalState();
  return S_OK;
}

HRESULT CGaiaCredentialBase::GetUserSid(wchar_t** sid) {
  *sid = nullptr;
  return S_FALSE;
}

HRESULT CGaiaCredentialBase::Initialize(IGaiaCredentialProvider* provider) {
  LOGFN(VERBOSE);
  DCHECK(provider);

  provider_ = provider;
  return S_OK;
}

HRESULT CGaiaCredentialBase::Terminate() {
  LOGFN(VERBOSE);
  SetDeselected();
  provider_.Reset();
  return S_OK;
}

void CGaiaCredentialBase::TerminateLogonProcess() {
  // Terminate login UI process if started.  This is best effort since it may
  // have already terminated.
  if (logon_ui_process_ != INVALID_HANDLE_VALUE) {
    LOGFN(VERBOSE) << "Attempting to kill logon UI process";
    ::TerminateProcess(logon_ui_process_, kUiecKilled);
    logon_ui_process_ = INVALID_HANDLE_VALUE;
  }
}

HRESULT CGaiaCredentialBase::ValidateOrCreateUser(
    const base::Value::Dict& result,
    BSTR* domain,
    BSTR* username,
    BSTR* sid,
    BSTR* error_text) {
  LOGFN(VERBOSE);
  DCHECK(domain);
  DCHECK(username);
  DCHECK(sid);
  DCHECK(error_text);
  DCHECK(sid);

  *error_text = nullptr;

  wchar_t found_username[kWindowsUsernameBufferLength];
  wchar_t found_domain[kWindowsDomainBufferLength];
  wchar_t found_sid[kWindowsSidBufferLength];
  bool is_consumer_account = false;
  std::wstring gaia_id;
  HRESULT hr = MakeUsernameForAccount(
      result, &gaia_id, found_username, std::size(found_username), found_domain,
      std::size(found_domain), found_sid, std::size(found_sid),
      &is_consumer_account, error_text);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "MakeUsernameForAccount hr=" << putHR(hr);
    return hr;
  }

  // Disallow consumer accounts when mdm enrollment is enabled and the global
  // flag to allow consumer accounts is not set.
  if (MdmEnrollmentEnabled() && is_consumer_account) {
    DWORD allow_consumer_accounts =
        GetGlobalFlagOrDefault(kRegMdmAllowConsumerAccounts, 0);

    if (allow_consumer_accounts == 0) {
      LOGFN(ERROR) << "Consumer accounts are not allowed mdm_aca="
                   << allow_consumer_accounts;
      *error_text = AllocErrorString(IDS_DISALLOWED_CONSUMER_EMAIL_BASE);
      return E_FAIL;
    }
  }

  // Validates the authenticated user to either login to an existing user
  // profile or fall back to creation of a new user profile. Below are few
  // workflows.
  //
  // 1.) Add user flow with no existing association, found_sid should be empty,
  //     falls through account creation
  // 2.) Reauth user flow with no existing association, found_sid should be
  //     empty, login attempt fails.
  // 3.) Add user flow with existing association, found_sid exists,
  //     logs into existing Windows account.
  // 4.) Reauth user flow with existing association, found_sid exists,
  //     logs into existing Windows account if found_sid matches reauth user
  //     sid.
  // 5.) Add user flow with cloud association, found_sid exists,
  //     logs into existing account.
  // 6.) Add/Reauth user flow with cloud association, found_sid exists,
  //     logs into existing account if found_sid matches reauth user sid.
  hr =
      ValidateExistingUser(found_username, found_domain, found_sid, error_text);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "ValidateExistingUser hr=" << putHR(hr);
    return hr;
  }

  // If an existing user associated to the gaia id or email address was found,
  // make sure that it is valid for this credential.
  if (found_sid[0]) {
    // Update the name on the OS account if authenticated user has a different
    // name.
    std::wstring os_account_fullname;
    hr = OSUserManager::Get()->GetUserFullname(found_domain, found_username,
                                               &os_account_fullname);
    if (SUCCEEDED(hr)) {
      std::wstring profile_fullname = GetDictString(result, kKeyFullname);
      if (os_account_fullname.compare(profile_fullname.c_str()) != 0) {
        hr = OSUserManager::Get()->SetUserFullname(found_domain, found_username,
                                                   profile_fullname.c_str());
        // Failing to set Windows account full name shouldn't fail login.
        if (FAILED(hr))
          LOGFN(ERROR) << "SetUserFullname hr=" << putHR(hr);
      }

      // Set disable password change policy here as well. This flow would
      // make sure password change is disabled even if any end user tries
      // to enable it via registry after user create or association flow.
      // Note: We donot fail the login flow if password policies were not
      // applied for unknown reasons.
      OSUserManager::Get()->SetDefaultPasswordChangePolicies(found_domain,
                                                             found_username);
    } else {
      LOGFN(ERROR) << "GetUserFullname hr=" << putHR(hr);
    }

    *username = ::SysAllocString(found_username);
    *domain = ::SysAllocString(found_domain);
    *sid = ::SysAllocString(found_sid);

    return S_OK;
  }

  DWORD cpus = 0;
  provider()->GetUsageScenario(&cpus);

  // New users creation is not allowed during work station unlock. This code
  // prevents users from being created when the "Other User" tile appears on the
  // lock screen through certain system policy settings. In this situation only
  // the user who locked the computer is allowed to sign in.
  if (cpus == CPUS_UNLOCK_WORKSTATION) {
    *error_text = AllocErrorString(IDS_INVALID_UNLOCK_WORKSTATION_USER_BASE);
    return HRESULT_FROM_WIN32(ERROR_LOGON_TYPE_NOT_GRANTED);
    // This code prevents users from being created when the "Other User" tile
    // appears on the sign in scenario and only 1 user is allowed to use this
    // system.
  } else if (!CGaiaCredentialProvider::CanNewUsersBeCreated(
                 static_cast<CREDENTIAL_PROVIDER_USAGE_SCENARIO>(cpus))) {
    *error_text = AllocErrorString(IDS_ADD_USER_DISALLOWED_BASE);
    return HRESULT_FROM_WIN32(ERROR_LOGON_TYPE_NOT_GRANTED);
  }

  std::wstring local_password = GetDictString(result, kKeyPassword);
  std::wstring local_fullname = GetDictString(result, kKeyFullname);
  std::wstring comment(GetStringResource(IDS_USER_ACCOUNT_COMMENT_BASE));
  hr = OSUserManager::Get()->CreateNewUser(
      found_username, local_password.c_str(), local_fullname.c_str(),
      comment.c_str(),
      /*add_to_users_group=*/true, kMaxUsernameAttempts, username, sid);
  SecurelyClearString(local_password);

  // May return user exists if this is the anonymous credential and the maximum
  // attempts to generate a new username has been reached.
  if (hr == HRESULT_FROM_WIN32(NERR_UserExists)) {
    LOGFN(ERROR) << "Could not find a new username based on desired username '"
                 << found_domain << "\\" << found_username
                 << "'. Maximum attempts reached.";
    *error_text = AllocErrorString(IDS_INTERNAL_ERROR_BASE);
    return hr;
  } else if (hr == HRESULT_FROM_WIN32(NERR_PasswordTooShort)) {
    LOGFN(ERROR) << "Password being used is too short as per the group "
                 << "policies set by your IT admin on this device.";
    *error_text = AllocErrorString(IDS_CREATE_USER_PASSWORD_TOO_SHORT_BASE);
  } else if (FAILED(hr)) {
    LOGFN(ERROR) << "Failed to create user '" << found_domain << "\\"
                 << found_username << "'. hr=" << putHR(hr);
    *error_text = AllocErrorString(IDS_INTERNAL_ERROR_BASE);
    return hr;
  }

  *domain = ::SysAllocString(found_domain);

  return hr;
}

HRESULT CGaiaCredentialBase::OnUserAuthenticated(BSTR authentication_info,
                                                 BSTR* status_text) {
  USES_CONVERSION;
  DCHECK(status_text);
  *status_text = nullptr;

  // Logon UI process is no longer needed and should already be finished by now
  // so clear the handle so that calls to HandleAutoLogon do not block further
  // processing thinking that there is still a logon process active.
  logon_ui_process_ = INVALID_HANDLE_VALUE;

  // Convert the string to a base::Dictionary and add the calculated username
  // to it to be passed to the PerformPostSigninActions process.
  std::string json_string;
  base::WideToUTF8(OLE2CW(authentication_info),
                   ::SysStringLen(authentication_info), &json_string);

  std::optional<base::Value::Dict> properties =
      base::JSONReader::ReadDict(json_string, base::JSON_ALLOW_TRAILING_COMMAS);

  SecurelyClearString(json_string);
  json_string.clear();

  if (!properties) {
    LOGFN(ERROR) << "base::JSONReader::Read failed to translate to JSON";
    *status_text = AllocErrorString(IDS_INVALID_UI_RESPONSE_BASE);
    return E_FAIL;
  }

  {
    HRESULT hr = ValidateResult(*properties, status_text);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "ValidateResult hr=" << putHR(hr);
      SecurelyClearDictionaryValue(properties);
      return hr;
    }

    const std::wstring email = GetDictString(*properties, kKeyEmail);
    const std::wstring email_domain = email.substr(email.find(L"@") + 1);
    const std::vector<std::wstring> allowed_domains = GetEmailDomainsList();

    if (!base::Contains(allowed_domains, email_domain)) {
      LOGFN(VERBOSE) << "Account " << email
                     << " isn't in a domain from allowed domains.";
      *status_text =
          CGaiaCredentialBase::AllocErrorString(IDS_INVALID_EMAIL_DOMAIN_BASE);
      SecurelyClearDictionaryValue(properties);
      return E_FAIL;
    }

    std::vector<std::wstring> permitted_accounts = GetPermittedAccounts();
    if (!permitted_accounts.empty() &&
        !base::Contains(permitted_accounts, email)) {
      *status_text = AllocErrorString(IDS_EMAIL_MISMATCH_BASE);
      SecurelyClearDictionaryValue(properties);
      return E_FAIL;
    }

    // The value in |dict| is now known to contain everything that is needed
    // from the GLS. Try to validate the user that wants to sign in and then
    // add additional information into |dict| as needed.
    hr = ValidateOrCreateUser(*properties, &domain_, &username_, &user_sid_,
                              status_text);
    if (FAILED(hr)) {
      // In case an error text isn't set in any failure path, have one to use as
      // the last resort.
      if (*status_text == nullptr)
        *status_text = AllocErrorString(IDS_INVALID_UI_RESPONSE_BASE);
      LOGFN(ERROR) << "ValidateOrCreateUser hr=" << putHR(hr);
      SecurelyClearDictionaryValue(properties);
      return hr;
    }

    authentication_results_ = std::move(properties);
    // Update the info whether the user is an AD joined user or local user.
    std::wstring sid = OLE2CW(user_sid_);
    authentication_results_->Set(
        kKeyIsAdJoinedUser,
        base::Value(OSUserManager::Get()->IsUserDomainJoined(sid) ? "true"
                                                                  : "false"));
  }

  std::wstring gaia_id = GetDictString(*authentication_results_, kKeyId);
  // TODO(crbug.com/41466886) Use downscoped token here.
  std::wstring access_token =
      GetDictString(*authentication_results_, kKeyAccessToken);
  GetUserConfigsIfStale(OLE2CW(user_sid_), gaia_id, access_token);
  SecurelyClearString(access_token);

  std::wstring local_password =
      GetDictString(*authentication_results_, kKeyPassword);
  password_ = ::SysAllocString(local_password.c_str());
  SecurelyClearString(local_password);

  // Disable the submit button. Either the signon will succeed with the given
  // credentials or a password update will be needed and that flow will handle
  // re-enabling the submit button in HandleAutoLogon.
  if (events_)
    events_->SetFieldInteractiveState(this, FID_SUBMIT, CPFIS_DISABLED);

  // Check if the credentials are valid for the user. If they aren't show the
  // password update prompt and continue without authenticating on the provider.
  if (!AreCredentialsValid()) {
    // Change UI into a mode where it expects to have the old password entered.
    std::wstring old_windows_password;
    needs_windows_password_ = true;

    // Pre-fill the old password if possible so that the sign in will proceed to
    // automatically update the password.
    if (SUCCEEDED(RecoverWindowsPasswordIfPossible(&old_windows_password))) {
      current_windows_password_ =
          ::SysAllocString(old_windows_password.c_str());
      SecurelyClearString(old_windows_password);
    } else {
      // Fall-through to continue with auto sign in and try the recovered
      // password.
      DisplayPasswordField(IDS_PASSWORD_UPDATE_NEEDED_BASE);
      return S_FALSE;
    }
  }

  result_status_ = STATUS_SUCCESS;

  // Prevent update of token handle validity until after sign in has completed
  // so the list of credentials doesn't suddenly change between now and when the
  // attempt to auto login occurs.
  PreventDenyAccessUpdate();

  // When this function returns, winlogon will be told to logon to the newly
  // created account.  This is important, as the save account info process
  // can't actually save the info until the user's profile is created, which
  // happens on first logon.
  return provider_->OnUserAuthenticated(static_cast<IGaiaCredential*>(this),
                                        username_, password_, user_sid_, TRUE);
}

HRESULT CGaiaCredentialBase::ReportError(LONG status,
                                         LONG substatus,
                                         BSTR status_text) {
  USES_CONVERSION;
  LOGFN(VERBOSE);

  // Provider may be unset if the GLS process ended as a result of a kill
  // request coming from Terminate() which would release the |provider_|
  // reference.
  if (!provider_)
    return S_OK;

  result_status_ = status;

  // If the user cancelled out of the logon, the process may be already
  // terminated, but if the handle to the process is still valid the
  // credential provider will not start a new GLS process when requested so
  // try to terminate the logon process now and clear the handle.
  TerminateLogonProcess();
  UpdateSubmitButtonInteractiveState();

  DisplayErrorInUI(status, STATUS_SUCCESS, status_text);

  return provider_->OnUserAuthenticated(nullptr, CComBSTR(), CComBSTR(),
                                        CComBSTR(), FALSE);
}

bool CGaiaCredentialBase::UpdateSubmitButtonInteractiveState() {
  bool should_enable =
      logon_ui_process_ == INVALID_HANDLE_VALUE &&
      ((!needs_windows_password_ || current_windows_password_.Length()) ||
       (needs_windows_password_ && request_force_password_change_));
  if (events_) {
    events_->SetFieldInteractiveState(
        this, FID_SUBMIT, should_enable ? CPFIS_NONE : CPFIS_DISABLED);
  }

  return should_enable;
}

void CGaiaCredentialBase::DisplayPasswordField(int password_message) {
  needs_windows_password_ = true;
  if (events_) {
    if (request_force_password_change_) {
      events_->SetFieldState(this, FID_CURRENT_PASSWORD_FIELD, CPFS_HIDDEN);
      events_->SetFieldString(
          this, FID_DESCRIPTION,
          GetStringResource(IDS_CONFIRM_FORCED_PASSWORD_CHANGE_BASE).c_str());
      events_->SetFieldString(
          this, FID_FORGOT_PASSWORD_LINK,
          GetStringResource(IDS_ENTER_PASSWORD_LINK_BASE).c_str());
      events_->SetFieldSubmitButton(this, FID_SUBMIT, FID_DESCRIPTION);
    } else {
      events_->SetFieldString(this, FID_DESCRIPTION,
                              GetStringResource(password_message).c_str());
      if (!BlockingPasswordError(password_message)) {
        events_->SetFieldState(this, FID_CURRENT_PASSWORD_FIELD,
                               CPFS_DISPLAY_IN_SELECTED_TILE);
        // Force password link won't be displayed if the machine is domain
        // joined or force reset password is disabled through registry.
        if (!OSUserManager::Get()->IsUserDomainJoined(get_sid().m_str) &&
            GetGlobalFlagOrDefault(kRegMdmEnableForcePasswordReset, 1)) {
          events_->SetFieldState(this, FID_FORGOT_PASSWORD_LINK,
                                 CPFS_DISPLAY_IN_SELECTED_TILE);
          events_->SetFieldString(
              this, FID_FORGOT_PASSWORD_LINK,
              GetStringResource(IDS_FORGOT_PASSWORD_LINK_BASE).c_str());
        }
        events_->SetFieldInteractiveState(this, FID_CURRENT_PASSWORD_FIELD,
                                          CPFIS_FOCUSED);
        events_->SetFieldSubmitButton(this, FID_SUBMIT,
                                      FID_CURRENT_PASSWORD_FIELD);
      }
    }
  }
}

HRESULT CGaiaCredentialBase::ValidateExistingUser(const std::wstring& username,
                                                  const std::wstring& domain,
                                                  const std::wstring& sid,
                                                  BSTR* error_text) {
  return S_OK;
}

HRESULT CGaiaCredentialBase::RecoverWindowsPasswordIfPossible(
    std::wstring* recovered_password) {
  DCHECK(recovered_password);

  if (!authentication_results_) {
    LOGFN(ERROR) << "No authentication results found during sign in";
    return E_FAIL;
  }

  const std::string* access_token =
      authentication_results_->FindString(kKeyAccessToken);
  if (!access_token) {
    LOGFN(ERROR) << "No access token found in authentication results";
    return E_FAIL;
  }

  return PasswordRecoveryManager::Get()->RecoverWindowsPasswordIfPossible(
      OLE2CW(get_sid()), *access_token, recovered_password);
}

}  // namespace credential_provider
