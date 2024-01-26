// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/common/gcp_strings.h"

#include "build/branding_buildflags.h"

namespace credential_provider {

// Names of keys returned on json data from UI process.
const char kKeyEmail[] = "email";
const char kKeyPicture[] = "picture";
const char kKeyFullname[] = "full_name";
const char kKeyId[] = "id";
const char kKeyMdmUrl[] = "mdm_url";
const char kKeyMdmIdToken[] = "mdm_id_token";
const char kKeyPassword[] = "password";
const char kKeyRefreshToken[] = "refresh_token";
const char kKeyAccessToken[] = "access_token";
const char kKeyMdmAccessToken[] = "mdm_access_token";
const char kKeySID[] = "sid";
const char kKeyTokenHandle[] = "token_handle";
const char kKeyUsername[] = "user_name";
const char kKeyDomain[] = "domain";
const char kKeyExitCode[] = "exit_code";

// AD attributes related to the device.
const char kKeyIsAdJoinedUser[] = "is_ad_joined_user";

// Name of registry value that holds user properties.
const wchar_t kUserTokenHandle[] = L"th";
const wchar_t kUserEmail[] = L"email";
const wchar_t kUserId[] = L"id";
const wchar_t kUserPictureUrl[] = L"pic";

// Username, password and sid key for special GAIA account to run GLS.
const wchar_t kDefaultGaiaAccountName[] = L"gaia";
// L$ prefix means this secret can only be accessed locally.
const wchar_t kLsaKeyGaiaUsername[] = L"L$GAIA_USERNAME";
const wchar_t kLsaKeyGaiaPassword[] = L"L$GAIA_PASSWORD";
const wchar_t kLsaKeyGaiaSid[] = L"L$GAIA_SID";

// These two variables need to remain consistent.
const wchar_t kDesktopName[] = L"Winlogon";
const wchar_t kDesktopFullName[] = L"WinSta0\\Winlogon";

// Google Update related registry paths.
#define GCPW_UPDATE_CLIENT_GUID L"{32987697-A14E-4B89-84D6-630D5431E831}"

const wchar_t kGcpwUpdateClientGuid[] = GCPW_UPDATE_CLIENT_GUID;

const wchar_t kRegUpdaterClientStateAppPath[] =
    L"SOFTWARE\\Google\\Update\\ClientState\\" GCPW_UPDATE_CLIENT_GUID;
const wchar_t kRegUpdaterClientsAppPath[] =
    L"SOFTWARE\\Google\\Update\\Clients\\" GCPW_UPDATE_CLIENT_GUID;
const wchar_t kRegUninstallStringField[] = L"UninstallString";
const wchar_t kRegUninstallArgumentsField[] = L"UninstallArguments";
const wchar_t kRegUsageStatsName[] = L"usagestats";
const wchar_t kRegUpdateTracksName[] = L"ap";
const wchar_t kRegVersionName[] = L"pv";

const wchar_t kRegUninstall[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
const wchar_t kRegUninstallProduct[] = L"GCPW";
const wchar_t kRegUninstallString[] = L"UninstallString";
const wchar_t kRegUninstallDisplayName[] = L"DisplayName";
const wchar_t kRegInstallLocation[] = L"InstallLocation";
const wchar_t kRegDisplayIcon[] = L"DisplayIcon";
const wchar_t kRegNoModify[] = L"NoModify";
const wchar_t kRegNoRepair[] = L"NoRepair";
const wchar_t kRegPublisherName[] = L"Publisher";
const wchar_t kRegVersion[] = L"Version";
const wchar_t kRegDisplayVersion[] = L"DisplayVersion";
const wchar_t kRegInstallDate[] = L"InstallDate";
const wchar_t kRegVersionMajor[] = L"VersionMajor";
const wchar_t kRegVersionMinor[] = L"VersionMinor";
const wchar_t kRegPublisher[] = L"Google LLC";

// Chrome is being opened to show the credential provider logon page.  This
// page is always shown in incognito mode.
const char kGcpwSigninSwitch[] = "gcpw-signin";

// The email to use to prefill the Gaia signin page.
const char kPrefillEmailSwitch[] = "prefill-email";

// Comma separated list of valid Gaia signin domains. If email that is signed
// into gaia is not part of these domains no LST will be minted and an error
// will be reported.
const char kEmailDomainsSwitch[] = "email-domains";

// Expected gaia-id of user that will be signing into gaia. If the ids do not
// match after signin, no LST will be minted and an error will be reported.
const char kGaiaIdSwitch[] = "gaia-id";

// Allows specification of the gaia endpoint to use to display the signin page
// for GCPW.
const char kGcpwEndpointPathSwitch[] = "gcpw-endpoint-path";

// Allows specifying additional oauth scopes for the access token being passed
// to GCPW.
const char kGcpwAdditionalOauthScopes[] = "gcpw-additional-oauth-scopes";

// The show_tos parameter is used to specify whether tos screen needs to be
// shown as part of the login process or not.
const char kShowTosSwitch[] = "show_tos";

// Parameter appended to sign in URL to pass valid signin domains to the inline
// login handler. These domains are separated by ','.
const char kEmailDomainsSigninPromoParameter[] = "emailDomains";
const char kEmailDomainsSeparator[] = ",";
const char kValidateGaiaIdSigninPromoParameter[] = "validate_gaia_id";
const char kGcpwEndpointPathPromoParameter[] = "gcpw_endpoint_path";

const wchar_t kRunAsCrashpadHandlerEntryPoint[] = L"RunAsCrashpadHandler";

// Flags to manipulate behavior of Chrome when importing credentials for the
// account signs in through GCPW.
const wchar_t kAllowImportOnlyOnFirstRun[] = L"allow_import_only_on_first_run";
const wchar_t kAllowImportWhenPrimaryAccountExists[] =
    L"allow_import_when_primary_exists";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const wchar_t kRegHkcuAccountsPath[] = L"Software\\Google\\Accounts";
#else
const wchar_t kRegHkcuAccountsPath[] = L"Software\\Chromium\\Accounts";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace credential_provider
