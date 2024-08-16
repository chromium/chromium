// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/error_page/common/localized_error.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/error_page/common/alt_game_images.h"
#include "components/error_page/common/error.h"
#include "components/error_page/common/error_page_switches.h"
#include "components/error_page/common/net_error_info.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/net_errors.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/origin.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace error_page {

namespace {

// Hardcode these constants to avoid dependences on //chrome and //content.
const char kChromeUIScheme[] = "chrome";
const char kChromeUIDinoHost[] = "dino";

static const char kRedirectLoopLearnMoreUrl[] =
    "https://support.google.com/chrome?p=rl_error";

enum NAV_SUGGESTIONS {
  SUGGEST_NONE = 0,
  SUGGEST_DIAGNOSE_TOOL = 1 << 0,
  SUGGEST_CHECK_CONNECTION = 1 << 1,
  SUGGEST_DNS_CONFIG = 1 << 2,
  SUGGEST_FIREWALL_CONFIG = 1 << 3,
  SUGGEST_PROXY_CONFIG = 1 << 4,
  SUGGEST_DISABLE_EXTENSION = 1 << 5,
  SUGGEST_LEARNMORE = 1 << 6,
  SUGGEST_CONTACT_ADMINISTRATOR = 1 << 7,
  SUGGEST_UNSUPPORTED_CIPHER = 1 << 8,
  SUGGEST_ANTIVIRUS_CONFIG = 1 << 9,
  SUGGEST_OFFLINE_CHECKS = 1 << 10,
  // Reload page suggestion for pages created by a post.
  SUGGEST_REPOST_RELOAD = 1 << 11,
  SUGGEST_NAVIGATE_TO_ORIGIN = 1 << 12,
  SUGGEST_SECURE_DNS_CONFIG = 1 << 13,
  SUGGEST_CAPTIVE_PORTAL_SIGNIN = 1 << 14,
  SUGGEST_RELOAD_PRIVATE_NETWORK_ACCESS = 1 << 15,
};

enum SHOW_BUTTONS {
  SHOW_NO_BUTTONS    = 0,
  SHOW_BUTTON_RELOAD = 1,
};

struct LocalizedErrorMap {
  int error_code;
  unsigned int heading_resource_id;
  // Detailed summary used when the error is in the main frame and shown on
  // mouse over when the error is in a frame.
  unsigned int summary_resource_id;
  int suggestions;  // Bitmap of SUGGEST_* values.
  int buttons;      // Bitmap of which buttons if any to show.
};

// clang-format off
const LocalizedErrorMap net_error_options[] = {
  {net::ERR_TIMED_OUT,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_TIMED_OUT,
   SUGGEST_CHECK_CONNECTION | SUGGEST_FIREWALL_CONFIG | SUGGEST_PROXY_CONFIG |
       SUGGEST_DIAGNOSE_TOOL | SUGGEST_CAPTIVE_PORTAL_SIGNIN,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_CONNECTION_TIMED_OUT,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_TIMED_OUT,
   SUGGEST_CHECK_CONNECTION | SUGGEST_FIREWALL_CONFIG | SUGGEST_PROXY_CONFIG |
       SUGGEST_DIAGNOSE_TOOL | SUGGEST_CAPTIVE_PORTAL_SIGNIN,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_CONNECTION_CLOSED,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_CONNECTION_CLOSED,
   SUGGEST_CHECK_CONNECTION | SUGGEST_FIREWALL_CONFIG | SUGGEST_PROXY_CONFIG |
       SUGGEST_DIAGNOSE_TOOL | SUGGEST_CAPTIVE_PORTAL_SIGNIN,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_CONNECTION_RESET,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_CONNECTION_RESET,
   SUGGEST_CHECK_CONNECTION | SUGGEST_FIREWALL_CONFIG | SUGGEST_PROXY_CONFIG |
       SUGGEST_DIAGNOSE_TOOL | SUGGEST_CAPTIVE_PORTAL_SIGNIN,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_CONNECTION_REFUSED,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_CONNECTION_REFUSED,
   SUGGEST_CHECK_CONNECTION | SUGGEST_FIREWALL_CONFIG | SUGGEST_PROXY_CONFIG |
       SUGGEST_CAPTIVE_PORTAL_SIGNIN,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_CONNECTION_FAILED,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_CONNECTION_FAILED,
   SUGGEST_DIAGNOSE_TOOL | SUGGEST_CAPTIVE_PORTAL_SIGNIN,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_NAME_NOT_RESOLVED,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NAME_NOT_RESOLVED,
   SUGGEST_CHECK_CONNECTION | SUGGEST_DNS_CONFIG | SUGGEST_FIREWALL_CONFIG |
   SUGGEST_DIAGNOSE_TOOL | SUGGEST_CAPTIVE_PORTAL_SIGNIN |
       SUGGEST_PROXY_CONFIG,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_ICANN_NAME_COLLISION,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_ICANN_NAME_COLLISION,
   SUGGEST_NONE,
   SHOW_NO_BUTTONS,
  },
  {net::ERR_ADDRESS_UNREACHABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_ADDRESS_UNREACHABLE,
   SUGGEST_DIAGNOSE_TOOL | SUGGEST_CAPTIVE_PORTAL_SIGNIN,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_NETWORK_ACCESS_DENIED,
   IDS_ERRORPAGES_HEADING_NETWORK_ACCESS_DENIED,
   IDS_ERRORPAGES_SUMMARY_NETWORK_ACCESS_DENIED,
   SUGGEST_CHECK_CONNECTION | SUGGEST_FIREWALL_CONFIG |
       SUGGEST_ANTIVIRUS_CONFIG | SUGGEST_DIAGNOSE_TOOL |
       SUGGEST_CAPTIVE_PORTAL_SIGNIN,
   SHOW_NO_BUTTONS,
  },
  {net::ERR_PROXY_CONNECTION_FAILED,
   IDS_ERRORPAGES_HEADING_INTERNET_DISCONNECTED,
   IDS_ERRORPAGES_SUMMARY_PROXY_CONNECTION_FAILED,
   SUGGEST_PROXY_CONFIG | SUGGEST_CONTACT_ADMINISTRATOR |
       SUGGEST_DIAGNOSE_TOOL | SUGGEST_CAPTIVE_PORTAL_SIGNIN,
   SHOW_NO_BUTTONS,
  },
  {net::ERR_INTERNET_DISCONNECTED,
   IDS_ERRORPAGES_HEADING_INTERNET_DISCONNECTED,
   IDS_ERRORPAGES_HEADING_INTERNET_DISCONNECTED,
   SUGGEST_OFFLINE_CHECKS | SUGGEST_DIAGNOSE_TOOL |
       SUGGEST_CAPTIVE_PORTAL_SIGNIN,
   SHOW_NO_BUTTONS,
  },
  {net::ERR_FILE_NOT_FOUND,
   IDS_ERRORPAGES_HEADING_FILE_NOT_FOUND,
   IDS_ERRORPAGES_SUMMARY_FILE_NOT_FOUND,
   SUGGEST_NONE,
   SHOW_NO_BUTTONS,
  },
  {net::ERR_UPLOAD_FILE_CHANGED,
   IDS_ERRORPAGES_HEADING_FILE_NOT_FOUND,
   IDS_ERRORPAGES_SUMMARY_FILE_NOT_FOUND,
   SUGGEST_NONE,
   SHOW_NO_BUTTONS,
  },
  {net::ERR_CACHE_MISS,
   IDS_ERRORPAGES_HEADING_CACHE_READ_FAILURE,
   IDS_ERRORPAGES_SUMMARY_CACHE_READ_FAILURE,
   SUGGEST_NONE,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_CACHE_READ_FAILURE,
   IDS_ERRORPAGES_HEADING_CACHE_READ_FAILURE,
   IDS_ERRORPAGES_SUMMARY_CACHE_READ_FAILURE,
   SUGGEST_NONE,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_NETWORK_IO_SUSPENDED,
   IDS_ERRORPAGES_HEADING_CONNECTION_INTERRUPTED,
   IDS_ERRORPAGES_SUMMARY_NETWORK_IO_SUSPENDED,
   SUGGEST_NONE,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_TOO_MANY_REDIRECTS,
   IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
   IDS_ERRORPAGES_SUMMARY_TOO_MANY_REDIRECTS,
   SUGGEST_LEARNMORE,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_EMPTY_RESPONSE,
   IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
   IDS_ERRORPAGES_SUMMARY_EMPTY_RESPONSE,
   SUGGEST_NONE,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_RESPONSE_HEADERS_MULTIPLE_CONTENT_LENGTH,
   IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
   IDS_ERRORPAGES_SUMMARY_INVALID_RESPONSE,
   SUGGEST_NONE,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_RESPONSE_HEADERS_MULTIPLE_CONTENT_DISPOSITION,
   IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
   IDS_ERRORPAGES_SUMMARY_INVALID_RESPONSE,
   SUGGEST_NONE,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_RESPONSE_HEADERS_MULTIPLE_LOCATION,
   IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
   IDS_ERRORPAGES_SUMMARY_INVALID_RESPONSE,
   SUGGEST_NONE,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_INVALID_HTTP_RESPONSE,
   IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
   IDS_ERRORPAGES_SUMMARY_INVALID_RESPONSE,
   SUGGEST_NONE,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_CONTENT_LENGTH_MISMATCH,
   IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
   IDS_ERRORPAGES_SUMMARY_CONNECTION_CLOSED,
   SUGGEST_NONE,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_INCOMPLETE_CHUNKED_ENCODING,
   IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
   IDS_ERRORPAGES_SUMMARY_CONNECTION_CLOSED,
   SUGGEST_NONE,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_INVALID_REDIRECT,
   IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
   IDS_ERRORPAGES_SUMMARY_INVALID_RESPONSE,
   SUGGEST_NONE,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_SSL_PROTOCOL_ERROR,
   IDS_ERRORPAGES_HEADING_INSECURE_CONNECTION,
   IDS_ERRORPAGES_SUMMARY_INVALID_RESPONSE,
   SUGGEST_DIAGNOSE_TOOL,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_BAD_SSL_CLIENT_AUTH_CERT,
   IDS_ERRORPAGES_HEADING_ACCESS_DENIED,
   IDS_ERRORPAGES_SUMMARY_BAD_SSL_CLIENT_AUTH_CERT,
   SUGGEST_CONTACT_ADMINISTRATOR,
   SHOW_NO_BUTTONS,
  },
  {net::ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED,
   IDS_ERRORPAGES_HEADING_ACCESS_DENIED,
   IDS_ERRORPAGES_SUMMARY_SSL_CLIENT_AUTH_SIGNATURE_FAILED,
   SUGGEST_CONTACT_ADMINISTRATOR,
   SHOW_NO_BUTTONS,
  },
  {net::ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY,
   IDS_ERRORPAGES_HEADING_ACCESS_DENIED,
   IDS_ERRORPAGES_SUMMARY_SSL_CLIENT_AUTH_SIGNATURE_FAILED,
   SUGGEST_CONTACT_ADMINISTRATOR,
   SHOW_NO_BUTTONS,
  },
  {net::ERR_SSL_CLIENT_AUTH_NO_COMMON_ALGORITHMS,
   IDS_ERRORPAGES_HEADING_ACCESS_DENIED,
   IDS_ERRORPAGES_SUMMARY_SSL_CLIENT_AUTH_SIGNATURE_FAILED,
   SUGGEST_CONTACT_ADMINISTRATOR,
   SHOW_NO_BUTTONS,
  },
  {net::ERR_SSL_CLIENT_AUTH_CERT_BAD_FORMAT,
   IDS_ERRORPAGES_HEADING_ACCESS_DENIED,
   IDS_ERRORPAGES_SUMMARY_SSL_CLIENT_AUTH_SIGNATURE_FAILED,
   SUGGEST_CONTACT_ADMINISTRATOR,
   SHOW_NO_BUTTONS,
  },
  {net::ERR_TLS13_DOWNGRADE_DETECTED,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_CONNECTION_FAILED,
   SUGGEST_CHECK_CONNECTION | SUGGEST_FIREWALL_CONFIG | SUGGEST_PROXY_CONFIG,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN,
   IDS_ERRORPAGES_HEADING_INSECURE_CONNECTION,
   IDS_CERT_ERROR_SUMMARY_PINNING_FAILURE_DETAILS,
   SUGGEST_NONE,
   SHOW_NO_BUTTONS,
  },
  {net::ERR_TEMPORARILY_THROTTLED,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE,
   SUGGEST_DISABLE_EXTENSION,
   SHOW_NO_BUTTONS,
  },
  {net::ERR_BLOCKED_BY_CLIENT,
   IDS_ERRORPAGES_HEADING_BLOCKED,
   IDS_ERRORPAGES_SUMMARY_BLOCKED_BY_CLIENT,
   SUGGEST_NONE,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
   IDS_ERRORPAGES_HEADING_BLOCKED,
   IDS_ERRORPAGES_SUMMARY_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
   SUGGEST_RELOAD_PRIVATE_NETWORK_ACCESS,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_BLOCKED_BY_CSP,
   IDS_ERRORPAGES_HEADING_BLOCKED,
   IDS_ERRORPAGES_SUMMARY_BLOCKED_BY_SECURITY,
   SUGGEST_NONE,
   SHOW_NO_BUTTONS,
  },
  {net::ERR_NETWORK_CHANGED,
   IDS_ERRORPAGES_HEADING_CONNECTION_INTERRUPTED,
   IDS_ERRORPAGES_SUMMARY_NETWORK_CHANGED,
   SUGGEST_NONE,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_BLOCKED_BY_ADMINISTRATOR,
   IDS_ERRORPAGES_HEADING_BLOCKED,
   IDS_ERRORPAGES_SUMMARY_BLOCKED_BY_ADMINISTRATOR,
   SUGGEST_NONE,
   SHOW_NO_BUTTONS,
  },
  {net::ERR_SSL_VERSION_OR_CIPHER_MISMATCH,
   IDS_ERRORPAGES_HEADING_INSECURE_CONNECTION,
   IDS_ERRORPAGES_SUMMARY_SSL_VERSION_OR_CIPHER_MISMATCH,
   SUGGEST_UNSUPPORTED_CIPHER,
   SHOW_NO_BUTTONS,
  },
  {net::ERR_SSL_OBSOLETE_CIPHER,
   IDS_ERRORPAGES_HEADING_INSECURE_CONNECTION,
   IDS_ERRORPAGES_SUMMARY_SSL_VERSION_OR_CIPHER_MISMATCH,
   SUGGEST_UNSUPPORTED_CIPHER,
   SHOW_NO_BUTTONS,
  },
  {net::ERR_SSL_SERVER_CERT_BAD_FORMAT,
   IDS_ERRORPAGES_HEADING_INSECURE_CONNECTION,
   IDS_ERRORPAGES_SUMMARY_SSL_SECURITY_ERROR,
   SUGGEST_NONE,
   SHOW_NO_BUTTONS,
  },
  {net::ERR_BLOCKED_BY_RESPONSE,
   IDS_ERRORPAGES_HEADING_BLOCKED,
   IDS_ERRORPAGES_SUMMARY_CONNECTION_REFUSED,
   SUGGEST_NONE,
   SHOW_NO_BUTTONS
  },
};
// clang-format on

// Special error page to be used in the case of navigating back to a page
// generated by a POST.  LocalizedError::HasStrings expects this net error code
// to also appear in the array above.
const LocalizedErrorMap repost_error = {
  net::ERR_CACHE_MISS,
  IDS_HTTP_POST_WARNING_TITLE,
  IDS_ERRORPAGES_HTTP_POST_WARNING,
  SUGGEST_REPOST_RELOAD,
  SHOW_NO_BUTTONS,
};

// Special error page to be used for hostname resolution errors that resulted
// from secure DNS network failures.  LocalizedError::HasStrings expects this
// net error code to also appear in the array above.
const LocalizedErrorMap secure_dns_network_error = {
    net::ERR_NAME_NOT_RESOLVED,
    IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
    IDS_ERRORPAGES_SUMMARY_NAME_NOT_RESOLVED,
    SUGGEST_CHECK_CONNECTION | SUGGEST_SECURE_DNS_CONFIG |
        SUGGEST_FIREWALL_CONFIG | SUGGEST_PROXY_CONFIG | SUGGEST_DIAGNOSE_TOOL,
    SHOW_BUTTON_RELOAD,
};

const LocalizedErrorMap http_error_options[] = {
    {
        403, IDS_ERRORPAGES_HEADING_ACCESS_DENIED,
        IDS_ERRORPAGES_SUMMARY_FORBIDDEN, SUGGEST_NONE, SHOW_BUTTON_RELOAD,
    },
    {
        404, IDS_ERRORPAGES_HEADING_NOT_FOUND, IDS_ERRORPAGES_SUMMARY_NOT_FOUND,
        SUGGEST_NONE, SHOW_BUTTON_RELOAD,
    },
    {
        410, IDS_ERRORPAGES_HEADING_NOT_FOUND, IDS_ERRORPAGES_SUMMARY_GONE,
        SUGGEST_NONE, SHOW_NO_BUTTONS,
    },

    {
        500, IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
        IDS_ERRORPAGES_SUMMARY_WEBSITE_CANNOT_HANDLE_REQUEST, SUGGEST_NONE,
        SHOW_BUTTON_RELOAD,
    },
    {
        501, IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
        IDS_ERRORPAGES_SUMMARY_WEBSITE_CANNOT_HANDLE_REQUEST, SUGGEST_NONE,
        SHOW_NO_BUTTONS,
    },
    {
        502, IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
        IDS_ERRORPAGES_SUMMARY_WEBSITE_CANNOT_HANDLE_REQUEST, SUGGEST_NONE,
        SHOW_BUTTON_RELOAD,
    },
    {
        503, IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
        IDS_ERRORPAGES_SUMMARY_WEBSITE_CANNOT_HANDLE_REQUEST, SUGGEST_NONE,
        SHOW_BUTTON_RELOAD,
    },
    {
        504, IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
        IDS_ERRORPAGES_SUMMARY_GATEWAY_TIMEOUT, SUGGEST_NONE,
        SHOW_BUTTON_RELOAD,
    },
};

const LocalizedErrorMap generic_4xx_5xx_error = {
    0,
    IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
    IDS_ERRORPAGES_SUMMARY_CONTACT_SITE_OWNER,
    SUGGEST_NONE,
    SHOW_BUTTON_RELOAD,
};

const LocalizedErrorMap dns_probe_error_options[] = {
    {
        error_page::DNS_PROBE_POSSIBLE,
        IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
        IDS_ERRORPAGES_SUMMARY_DNS_PROBE_RUNNING,
        SUGGEST_DIAGNOSE_TOOL,
        SHOW_BUTTON_RELOAD,
    },

    // DNS_PROBE_NOT_RUN is not here; NetErrorHelper will restore the original
    // error, which might be one of several DNS-related errors.

    {
        error_page::DNS_PROBE_STARTED,
        IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
        IDS_ERRORPAGES_SUMMARY_DNS_PROBE_RUNNING,
        // Include SUGGEST_RELOAD so the More button doesn't jump when we
        // update.
        SUGGEST_DIAGNOSE_TOOL,
        SHOW_BUTTON_RELOAD,
    },

    // DNS_PROBE_FINISHED_UNKNOWN is not here; NetErrorHelper will restore the
    // original error, which might be one of several DNS-related errors.

    {
        error_page::DNS_PROBE_FINISHED_NO_INTERNET,
        IDS_ERRORPAGES_HEADING_INTERNET_DISCONNECTED,
        IDS_ERRORPAGES_HEADING_INTERNET_DISCONNECTED,
        SUGGEST_OFFLINE_CHECKS | SUGGEST_DIAGNOSE_TOOL,
        SHOW_NO_BUTTONS,
    },
    {
        error_page::DNS_PROBE_FINISHED_BAD_CONFIG,
        IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
        IDS_ERRORPAGES_SUMMARY_NAME_NOT_RESOLVED,
        SUGGEST_DNS_CONFIG | SUGGEST_FIREWALL_CONFIG | SUGGEST_PROXY_CONFIG |
            SUGGEST_DIAGNOSE_TOOL,
        SHOW_BUTTON_RELOAD,
    },
    {
        error_page::DNS_PROBE_FINISHED_BAD_SECURE_CONFIG,
        IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
        IDS_ERRORPAGES_SUMMARY_NAME_NOT_RESOLVED,
        SUGGEST_SECURE_DNS_CONFIG | SUGGEST_FIREWALL_CONFIG |
            SUGGEST_PROXY_CONFIG | SUGGEST_DIAGNOSE_TOOL,
        SHOW_BUTTON_RELOAD,
    },
    {
        error_page::DNS_PROBE_FINISHED_NXDOMAIN,
        IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
        IDS_ERRORPAGES_CHECK_TYPO_SUMMARY,
        SUGGEST_DIAGNOSE_TOOL | SUGGEST_CAPTIVE_PORTAL_SIGNIN,
        SHOW_BUTTON_RELOAD,
    },
};

const LocalizedErrorMap link_preview_error_options[] = {
    {
        error_page::LinkPreviewErrorCode::kNonHttpsForbidden,
        IDS_ERRORPAGES_HEADING_LINKPREVIEW_NON_HTTPS_FORBIDDEN,
        IDS_ERRORPAGES_SUMMARY_LINKPREVIEW_NON_HTTPS_FORBIDDEN,
        SUGGEST_NONE,
        SHOW_NO_BUTTONS,
    },
};

const LocalizedErrorMap* FindErrorMapInArray(const LocalizedErrorMap* maps,
                                                   size_t num_maps,
                                                   int error_code) {
  for (size_t i = 0; i < num_maps; ++i) {
    if (maps[i].error_code == error_code)
      return &maps[i];
  }
  return nullptr;
}

const LocalizedErrorMap* LookupErrorMap(const std::string& error_domain,
                                        int error_code,
                                        bool is_secure_dns_network_error,
                                        bool is_post) {
  if (error_domain == Error::kNetErrorDomain) {
    // Display a different page in the special case of navigating through the
    // history to an uncached page created by a POST.
    if (is_post && error_code == net::ERR_CACHE_MISS)
      return &repost_error;
    // Display a different page in the special case of achieving a hostname
    // resolution error that was the result of a secure DNS network failure.
    if (is_secure_dns_network_error &&
        net::IsHostnameResolutionError(error_code)) {
      return &secure_dns_network_error;
    }
    return FindErrorMapInArray(net_error_options, std::size(net_error_options),
                               error_code);
  } else if (error_domain == Error::kHttpErrorDomain) {
    const LocalizedErrorMap* map = FindErrorMapInArray(
        http_error_options, std::size(http_error_options), error_code);
    // Handle miscellaneous 400/500 errors.
    return !map && error_code >= 400 && error_code < 600
               ? &generic_4xx_5xx_error
               : map;
  } else if (error_domain == Error::kDnsProbeErrorDomain) {
    const LocalizedErrorMap* map =
        FindErrorMapInArray(dns_probe_error_options,
                            std::size(dns_probe_error_options), error_code);
    DCHECK(map);
    return map;
  } else if (error_domain == Error::kLinkPreviewErrorDomain) {
    const LocalizedErrorMap* map =
        FindErrorMapInArray(link_preview_error_options,
                            std::size(link_preview_error_options), error_code);
    CHECK(map);
    return map;
  } else {
    NOTREACHED();
  }
}

// Returns a dictionary containing the strings for the settings menu under the
// app menu, and the advanced settings button.
base::Value::Dict GetStandardMenuItemsText() {
  base::Value::Dict standard_menu_items_text;
  standard_menu_items_text.Set("settingsTitle",
                               l10n_util::GetStringUTF16(IDS_SETTINGS_TITLE));
  standard_menu_items_text.Set(
      "advancedTitle",
      l10n_util::GetStringUTF16(IDS_SETTINGS_SHOW_ADVANCED_SETTINGS));
  return standard_menu_items_text;
}

// Gets the icon class for a given |error_domain| and |error_code|.
const char* GetIconClassForError(const std::string& error_domain,
                                 int error_code) {
  return LocalizedError::IsOfflineError(error_domain, error_code)
             ? "icon-offline"
         : error_code == net::ERR_BLOCKED_BY_ADMINISTRATOR ? "icon-info"
                                                           : "icon-generic";
}

base::Value::Dict SingleEntryDictionary(std::string_view path, int message_id) {
  base::Value::Dict result;
  result.Set(path, l10n_util::GetStringUTF16(message_id));
  return result;
}

// Adds a linked suggestion dictionary entry to the suggestions list.
void AddLinkedSuggestionToList(const int error_code,
                               const std::string& locale,
                               base::Value::List& suggestions_summary_list,
                               bool standalone_suggestion) {
  GURL learn_more_url;
  std::u16string suggestion_string =
      standalone_suggestion
          ? l10n_util::GetStringUTF16(
                IDS_ERRORPAGES_SUGGESTION_LEARNMORE_SUMMARY_STANDALONE)
          : l10n_util::GetStringUTF16(
                IDS_ERRORPAGES_SUGGESTION_LEARNMORE_SUMMARY);

  switch (error_code) {
    case net::ERR_TOO_MANY_REDIRECTS:
      learn_more_url = GURL(kRedirectLoopLearnMoreUrl);
      suggestion_string = l10n_util::GetStringUTF16(
          IDS_ERRORPAGES_SUGGESTION_DELETE_COOKIES_SUMMARY);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  DCHECK(learn_more_url.is_valid());
  // Add the language parameter to the URL.
  std::string query = learn_more_url.query() + "&hl=" + locale;
  GURL::Replacements repl;
  repl.SetQueryStr(query);
  GURL learn_more_url_with_locale = learn_more_url.ReplaceComponents(repl);

  base::Value::Dict suggestion_list_item;
  suggestion_list_item.Set("summary", suggestion_string);
  suggestion_list_item.Set("learnMoreUrl", learn_more_url_with_locale.spec());
  suggestions_summary_list.Append(std::move(suggestion_list_item));
}

// Check if a suggestion is in the bitmap of suggestions.
bool IsSuggested(int suggestions, int suggestion) {
  return !!(suggestions & suggestion);
}

// Check suggestion is the only item in the suggestions bitmap.
bool IsOnlySuggestion(int suggestions, int suggestion) {
  return IsSuggested(suggestions, suggestion) && !(suggestions & ~suggestion);
}

// Creates a list of suggestions that a user may try to resolve a particular
// network error. Appears above the fold underneath heading and intro paragraph.
void GetSuggestionsSummaryList(int error_code,
                               base::Value::Dict& error_strings,
                               int suggestions,
                               const std::string& locale,
                               base::Value::List& suggestions_summary_list,
                               bool can_show_network_diagnostics_dialog,
                               const GURL& failed_url,
                               const base::Value::Dict* error_page_params) {
  // Remove the diagnostic tool suggestion if the platform doesn't support it
  // or the url isn't valid.
  if (!can_show_network_diagnostics_dialog || !failed_url.is_valid() ||
      !failed_url.SchemeIsHTTPOrHTTPS()) {
    suggestions &= ~SUGGEST_DIAGNOSE_TOOL;
  }
  // Remove the captive portal signin suggestion if not in a portal state.
  if (!error_page_params ||
      !error_page_params->FindBool(kIsPortalStateKey).value_or(false)) {
    suggestions &= ~SUGGEST_CAPTIVE_PORTAL_SIGNIN;
  }

  if (suggestions == SUGGEST_NONE)
    return;

  if (IsOnlySuggestion(suggestions, SUGGEST_CONTACT_ADMINISTRATOR)) {
    DCHECK(suggestions_summary_list.empty());
    DCHECK(!(suggestions & ~SUGGEST_CONTACT_ADMINISTRATOR));
    suggestions_summary_list.Append(SingleEntryDictionary(
        "summary", IDS_ERRORPAGES_SUGGESTION_CONTACT_ADMIN_SUMMARY_STANDALONE));
    return;
  }
  if (IsSuggested(suggestions, SUGGEST_CONTACT_ADMINISTRATOR)) {
    suggestions_summary_list.Append(SingleEntryDictionary(
        "summary", IDS_ERRORPAGES_SUGGESTION_CONTACT_ADMIN_SUMMARY));
  }

  if (IsOnlySuggestion(suggestions,SUGGEST_REPOST_RELOAD)) {
    DCHECK(suggestions_summary_list.empty());
    DCHECK(!(suggestions & ~SUGGEST_REPOST_RELOAD));
    // If the page was created by a post, it can't be reloaded in the same
    // way, so just add a suggestion instead.
    // TODO(mmenke):  Make the reload button bring up the repost confirmation
    //                dialog for pages resulting from posts.
    suggestions_summary_list.Append(SingleEntryDictionary(
        "summary", IDS_ERRORPAGES_SUGGESTION_RELOAD_REPOST_SUMMARY));
    return;
  }
  DCHECK(!IsSuggested(suggestions, SUGGEST_REPOST_RELOAD));

  if (IsSuggested(suggestions, SUGGEST_RELOAD_PRIVATE_NETWORK_ACCESS)) {
    suggestions_summary_list.Append(SingleEntryDictionary(
        "summary", IDS_ERRORPAGES_SUGGESTION_RELOAD_PRIVATE_NETWORK_ACCESS));
  }

  if (IsOnlySuggestion(suggestions, SUGGEST_NAVIGATE_TO_ORIGIN)) {
    DCHECK(suggestions_summary_list.empty());
    DCHECK(!(suggestions & ~SUGGEST_NAVIGATE_TO_ORIGIN));
    url::Origin failed_origin = url::Origin::Create(failed_url);
    if (failed_origin.opaque())
      return;

    base::Value::Dict suggestion;
    suggestion.Set("summary",
                   l10n_util::GetStringUTF16(
                       IDS_ERRORPAGES_SUGGESTION_NAVIGATE_TO_ORIGIN));
    suggestion.Set("originURL", failed_origin.Serialize());
    suggestions_summary_list.Append(std::move(suggestion));
    return;
  }
  DCHECK(!IsSuggested(suggestions, SUGGEST_NAVIGATE_TO_ORIGIN));

  if (IsOnlySuggestion(suggestions, SUGGEST_LEARNMORE)) {
    DCHECK(suggestions_summary_list.empty());
    AddLinkedSuggestionToList(error_code, locale, suggestions_summary_list,
                              true);
    return;
  }
  if (IsSuggested(suggestions, SUGGEST_LEARNMORE)) {
    AddLinkedSuggestionToList(error_code, locale, suggestions_summary_list,
                              false);
  }

  if (suggestions & SUGGEST_CAPTIVE_PORTAL_SIGNIN) {
    suggestions_summary_list.Append(SingleEntryDictionary(
        "summary", IDS_ERRORPAGES_SUGGESTION_CAPTIVE_PORTAL_SIGNIN));
  }

  if (suggestions & SUGGEST_DISABLE_EXTENSION) {
    DCHECK(suggestions_summary_list.empty());
    suggestions_summary_list.Append(SingleEntryDictionary(
        "summary", IDS_ERRORPAGES_SUGGESTION_DISABLE_EXTENSION_SUMMARY));
    return;
  }
  DCHECK(!IsSuggested(suggestions, SUGGEST_DISABLE_EXTENSION));

  if (suggestions & SUGGEST_CHECK_CONNECTION) {
    suggestions_summary_list.Append(SingleEntryDictionary(
        "summary", IDS_ERRORPAGES_SUGGESTION_CHECK_CONNECTION_SUMMARY));
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (IsSuggested(suggestions, SUGGEST_DNS_CONFIG) &&
      IsSuggested(suggestions, SUGGEST_FIREWALL_CONFIG) &&
      IsSuggested(suggestions, SUGGEST_PROXY_CONFIG)) {
    suggestions_summary_list.Append(SingleEntryDictionary(
        "summary", IDS_ERRORPAGES_SUGGESTION_CHECK_PROXY_FIREWALL_DNS_SUMMARY));
  } else if (IsSuggested(suggestions, SUGGEST_SECURE_DNS_CONFIG) &&
             IsSuggested(suggestions, SUGGEST_FIREWALL_CONFIG) &&
             IsSuggested(suggestions, SUGGEST_PROXY_CONFIG)) {
    suggestions_summary_list.Append(SingleEntryDictionary(
        "summary",
        IDS_ERRORPAGES_SUGGESTION_CHECK_PROXY_FIREWALL_SECURE_DNS_SUMMARY));
  } else if (IsSuggested(suggestions, SUGGEST_FIREWALL_CONFIG) &&
             IsSuggested(suggestions, SUGGEST_ANTIVIRUS_CONFIG)) {
    suggestions_summary_list.Append(SingleEntryDictionary(
        "summary", IDS_ERRORPAGES_SUGGESTION_CHECK_FIREWALL_ANTIVIRUS_SUMMARY));
  } else if (IsSuggested(suggestions, SUGGEST_PROXY_CONFIG) &&
             IsSuggested(suggestions, SUGGEST_FIREWALL_CONFIG)) {
    suggestions_summary_list.Append(SingleEntryDictionary(
        "summary", IDS_ERRORPAGES_SUGGESTION_CHECK_PROXY_FIREWALL_SUMMARY));
  } else if (IsSuggested(suggestions, SUGGEST_PROXY_CONFIG)) {
    suggestions_summary_list.Append(SingleEntryDictionary(
        "summary", IDS_ERRORPAGES_SUGGESTION_CHECK_PROXY_ADDRESS_SUMMARY));
  } else {
    DCHECK(!(suggestions & SUGGEST_PROXY_CONFIG));
    DCHECK(!(suggestions & SUGGEST_FIREWALL_CONFIG));
    DCHECK(!(suggestions & SUGGEST_DNS_CONFIG));
    DCHECK(!(suggestions & SUGGEST_SECURE_DNS_CONFIG));
  }
#elif BUILDFLAG(IS_ANDROID)
  if (IsSuggested(suggestions, SUGGEST_SECURE_DNS_CONFIG)) {
    suggestions_summary_list.Append(SingleEntryDictionary(
        "summary", IDS_ERRORPAGES_SUGGESTION_CHECK_SECURE_DNS_SUMMARY));
  }
#endif

  if (IsSuggested(suggestions, SUGGEST_OFFLINE_CHECKS)) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    suggestions_summary_list.Append(SingleEntryDictionary(
        "summary", IDS_ERRORPAGES_SUGGESTION_TURN_OFF_AIRPLANE_SUMMARY));
    suggestions_summary_list.Append(SingleEntryDictionary(
        "summary", IDS_ERRORPAGES_SUGGESTION_TURN_ON_DATA_SUMMARY));
    suggestions_summary_list.Append(SingleEntryDictionary(
        "summary", IDS_ERRORPAGES_SUGGESTION_CHECKING_SIGNAL_SUMMARY));
#else
    suggestions_summary_list.Append(SingleEntryDictionary(
        "summary", IDS_ERRORPAGES_SUGGESTION_CHECK_HARDWARE_SUMMARY));
    suggestions_summary_list.Append(SingleEntryDictionary(
        "summary", IDS_ERRORPAGES_SUGGESTION_CHECK_WIFI_SUMMARY));
#endif
  }

// If the current platform has a directly accesible network diagnostics tool and
// the URL is valid add a suggestion.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  if (IsOnlySuggestion(suggestions, SUGGEST_DIAGNOSE_TOOL)) {
    int diagose_message_id =
        error_code == error_page::DNS_PROBE_FINISHED_NXDOMAIN
            ? IDS_ERRORPAGES_SUGGESTION_DIAGNOSE_CHECK_TYPO_STANDALONE
            : IDS_ERRORPAGES_SUGGESTION_DIAGNOSE_STANDALONE;

    suggestions_summary_list.Append(
        SingleEntryDictionary("summary", diagose_message_id));
    return;
  }
  if (IsSuggested(suggestions, SUGGEST_DIAGNOSE_TOOL)) {
    suggestions_summary_list.Append(
        SingleEntryDictionary("summary", IDS_ERRORPAGES_SUGGESTION_DIAGNOSE));
  }
#else
  DCHECK(!IsSuggested(suggestions, SUGGEST_DIAGNOSE_TOOL));
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

  // Add list prefix header.
  error_strings.Set(
      "suggestionsSummaryListHeader",
      l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUGGESTION_LIST_HEADER));
}

// Creates a dictionary with "header" and "body" entries and adds it to `list`.
void AddSuggestionDetailDictionaryToList(base::Value::List& list,
                                         int header_message_id,
                                         int body_message_id,
                                         bool append_standard_menu_items) {
  base::Value::Dict suggestion_list_item;
  if (append_standard_menu_items)
    suggestion_list_item = GetStandardMenuItemsText();

  if (header_message_id) {
    suggestion_list_item.Set("header",
                             l10n_util::GetStringUTF16(header_message_id));
  }
  if (body_message_id) {
    suggestion_list_item.Set("body",
                             l10n_util::GetStringUTF16(body_message_id));
  }
  list.Append(std::move(suggestion_list_item));
}

// Certain suggestions have supporting details which get displayed under
// the "Details" button.
void AddSuggestionsDetails(int error_code,
                           int suggestions,
                           base::Value::List& suggestions_details) {
  if (suggestions & SUGGEST_CHECK_CONNECTION) {
    AddSuggestionDetailDictionaryToList(suggestions_details,
          IDS_ERRORPAGES_SUGGESTION_CHECK_CONNECTION_HEADER,
          IDS_ERRORPAGES_SUGGESTION_CHECK_CONNECTION_BODY, false);
  }

#if !BUILDFLAG(IS_IOS)
  if (suggestions & SUGGEST_SECURE_DNS_CONFIG) {
    AddSuggestionDetailDictionaryToList(
        suggestions_details, IDS_ERRORPAGES_SUGGESTION_SECURE_DNS_CONFIG_HEADER,
        IDS_ERRORPAGES_SUGGESTION_SECURE_DNS_CONFIG_BODY, true);
  }
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (suggestions & SUGGEST_DNS_CONFIG) {
    AddSuggestionDetailDictionaryToList(suggestions_details,
          IDS_ERRORPAGES_SUGGESTION_DNS_CONFIG_HEADER,
          IDS_ERRORPAGES_SUGGESTION_DNS_CONFIG_BODY, false);

    AddSuggestionDetailDictionaryToList(
        suggestions_details,
        IDS_ERRORPAGES_SUGGESTION_NETWORK_PREDICTION_HEADER,
        IDS_ERRORPAGES_SUGGESTION_NETWORK_PREDICTION_BODY, true);
    suggestions_details.back().GetDict().Set(
        "noNetworkPredictionTitle",
        l10n_util::GetStringUTF16(IDS_NETWORK_PREDICTION_ENABLED_DESCRIPTION));
  }

  if (suggestions & SUGGEST_FIREWALL_CONFIG) {
    AddSuggestionDetailDictionaryToList(suggestions_details,
        IDS_ERRORPAGES_SUGGESTION_FIREWALL_CONFIG_HEADER,
        IDS_ERRORPAGES_SUGGESTION_FIREWALL_CONFIG_BODY, false);
  }

  // TODO(crbug.com/40199702): Provide meaningful strings for Fuchsia.
#if !BUILDFLAG(IS_FUCHSIA)
  if (suggestions & SUGGEST_PROXY_CONFIG) {
    AddSuggestionDetailDictionaryToList(
        suggestions_details, IDS_ERRORPAGES_SUGGESTION_PROXY_CONFIG_HEADER, 0,
        true);

    // Custom body string.
    suggestions_details.back().GetDict().Set(
        "body", l10n_util::GetStringFUTF16(
                    IDS_ERRORPAGES_SUGGESTION_PROXY_CONFIG_BODY,
                    l10n_util::GetStringUTF16(
                        IDS_ERRORPAGES_SUGGESTION_PROXY_DISABLE_PLATFORM)));
    suggestions_details.back().GetDict().Set(
        "proxyTitle",
        l10n_util::GetStringUTF16(IDS_OPTIONS_PROXIES_CONFIGURE_BUTTON));
  }
#endif  //  !BUILDFLAG(IS_FUCHSIA)
#endif

  if (suggestions & SUGGEST_CONTACT_ADMINISTRATOR &&
      error_code == net::ERR_BLOCKED_BY_ADMINISTRATOR) {
    AddSuggestionDetailDictionaryToList(suggestions_details,
        IDS_ERRORPAGES_SUGGESTION_VIEW_POLICIES_HEADER,
        IDS_ERRORPAGES_SUGGESTION_VIEW_POLICIES_BODY, false);
  }

  if (suggestions & SUGGEST_UNSUPPORTED_CIPHER) {
    AddSuggestionDetailDictionaryToList(suggestions_details,
        IDS_ERRORPAGES_SUGGESTION_UNSUPPORTED_CIPHER_HEADER,
        IDS_ERRORPAGES_SUGGESTION_UNSUPPORTED_CIPHER_BODY, false);
  }
}

std::string HttpErrorCodeToString(int error) {
  return std::string("HTTP ERROR ") + base::NumberToString(error);
}

}  // namespace

LocalizedError::PageState::PageState() = default;
LocalizedError::PageState::~PageState() = default;
LocalizedError::PageState::PageState(PageState&& other) = default;
LocalizedError::PageState& LocalizedError::PageState::operator=(
    PageState&& other) = default;

LocalizedError::PageState LocalizedError::GetPageState(
    int error_code,
    const std::string& error_domain,
    const GURL& failed_url,
    bool is_post,
    bool is_secure_dns_network_error,
    bool stale_copy_in_cache,
    bool can_show_network_diagnostics_dialog,
    bool is_incognito,
    bool offline_content_feature_enabled,
    bool auto_fetch_feature_enabled,
    bool is_kiosk_mode,
    const std::string& locale,
    bool is_blocked_by_extension,
    const base::Value::Dict* error_page_params) {
  LocalizedError::PageState result;
  if (LocalizedError::IsOfflineError(error_domain, error_code)) {
    result.is_offline_error = true;

    // These strings are to be read by a screen reader during the dino game.
    result.strings.Set(
        "dinoGameA11yAriaLabel",
        l10n_util::GetStringUTF16(IDS_ERRORPAGE_DINO_ARIA_LABEL));
    result.strings.Set("dinoGameA11yGameOver",
                       l10n_util::GetStringUTF16(IDS_ERRORPAGE_DINO_GAME_OVER));
    result.strings.Set(
        "dinoGameA11yHighScore",
        l10n_util::GetStringUTF16(IDS_ERRORPAGE_DINO_HIGH_SCORE));
    result.strings.Set("dinoGameA11yJump",
                       l10n_util::GetStringUTF16(IDS_ERRORPAGE_DINO_JUMP));
    result.strings.Set(
        "dinoGameA11yStartGame",
        l10n_util::GetStringUTF16(IDS_ERRORPAGE_DINO_GAME_START));
    result.strings.Set(
        "dinoGameA11ySpeedToggle",
        l10n_util::GetStringUTF16(IDS_ERRORPAGE_DINO_SLOW_SPEED_TOGGLE));
    result.strings.Set(
        "dinoGameA11yDescription",
        l10n_util::GetStringUTF16(IDS_ERRORPAGE_DINO_GAME_DESCRIPTION));

    if (EnableAltGameMode()) {
      result.strings.Set("enableAltGameMode", true);
      // We don't know yet which scale the page will use, so both 1x and 2x
      // should be loaded.
      result.strings.Set("altGameCommonImage1x",
                         GetAltGameImage(/*image_id=*/0, /*scale=*/1));
      result.strings.Set("altGameCommonImage2x",
                         GetAltGameImage(/*image_id=*/0, /*scale=*/2));
      int choice = ChooseAltGame();
      result.strings.Set("altGameType", base::NumberToString(choice));
      result.strings.Set("altGameSpecificImage1x", GetAltGameImage(choice, 1));
      result.strings.Set("altGameSpecificImage2x", GetAltGameImage(choice, 2));
    }
  }

  webui::SetLoadTimeDataDefaults(locale, &result.strings);

  bool show_game_instructions = failed_url.host() == kChromeUIDinoHost &&
                                failed_url.scheme() == kChromeUIScheme;

  // Grab the strings and settings that depend on the error type.  Init
  // options with default values.
  LocalizedErrorMap options = {
    0,
    IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
    IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE,
    SUGGEST_NONE,
    SHOW_NO_BUTTONS,
  };

  const LocalizedErrorMap* error_map = LookupErrorMap(
      error_domain, error_code, is_secure_dns_network_error, is_post);
  if (error_map)
    options = *error_map;

  // If we got "access denied" but the url was a file URL, then we say it was a
  // file instead of just using the "not available" default message. Just adding
  // ERR_ACCESS_DENIED to the map isn't sufficient, since that message may be
  // generated by some OSs when the operation doesn't involve a file URL.
  if (error_domain == Error::kNetErrorDomain &&
      error_code == net::ERR_ACCESS_DENIED && failed_url.scheme() == "file") {
    options.heading_resource_id = IDS_ERRORPAGES_HEADING_FILE_ACCESS_DENIED;
    options.summary_resource_id = IDS_ERRORPAGES_SUMMARY_FILE_ACCESS_DENIED;
    options.suggestions = SUGGEST_NONE;
    options.buttons = SHOW_BUTTON_RELOAD;
  }

  // Do not show any suggestions with links while in kiosk mode.
  if (is_kiosk_mode) {
    options.suggestions &= ~SUGGEST_DIAGNOSE_TOOL;
    options.suggestions &= ~SUGGEST_LEARNMORE;
    options.suggestions &= ~SUGGEST_CONTACT_ADMINISTRATOR;
  }

  std::u16string failed_url_string(url_formatter::FormatUrl(
      failed_url, url_formatter::kFormatUrlOmitNothing,
      base::UnescapeRule::NORMAL, nullptr, nullptr, nullptr));
  // URLs are always LTR.
  if (base::i18n::IsRTL())
    base::i18n::WrapStringWithLTRFormatting(&failed_url_string);

  std::u16string host_name(url_formatter::IDNToUnicode(failed_url.host()));
  if (failed_url.SchemeIsHTTPOrHTTPS()) {
    result.strings.Set("title", host_name);
  } else {
    result.strings.Set("title", failed_url_string);

    // If the page is blocked by policy, and no hostname is available to show,
    // instead show the scheme.
    if (error_code == net::ERR_BLOCKED_BY_ADMINISTRATOR && host_name.empty()) {
      options.heading_resource_id = IDS_ERRORPAGES_HEADING_BLOCKED_SCHEME;
      host_name = base::UTF8ToUTF16(failed_url.scheme());
    }
  }

  result.strings.Set("iconClass",
                     GetIconClassForError(error_domain, error_code));

  base::Value::Dict heading;

  int msg_id = show_game_instructions ? IDS_ERRORPAGES_GAME_INSTRUCTIONS
                                      : options.heading_resource_id;
  heading.Set("msg", l10n_util::GetStringUTF16(msg_id));
  heading.Set("hostName", host_name);
  result.strings.Set("heading", std::move(heading));

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // Check if easter egg should be disabled.
  if (command_line->HasSwitch(
          error_page::switches::kDisableDinosaurEasterEgg)) {
    // The presence of this string disables the easter egg. Acts as a flag.
    result.strings.Set("disabledEasterEgg",
                       l10n_util::GetStringUTF16(IDS_ERRORPAGE_FUN_DISABLED));
  }

  // Return early and don't add suggestions or other information when showing
  // game instructions.
  if (show_game_instructions) {
    // When showing instructions, set an empty error to prevent a "NULL" string.
    result.strings.Set("errorCode", "");
    return result;
  }

  base::Value::Dict summary;

  // Set summary message under the heading.
  std::u16string message;
  if (is_blocked_by_extension) {
    // Use a custom message if an extension blocked the request.
    message =
        l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUMMARY_BLOCKED_BY_EXTENSION);
    options.suggestions = SUGGEST_DISABLE_EXTENSION;
  } else {
    message = l10n_util::GetStringUTF16(options.summary_resource_id);
  }

  summary.Set("msg", std::move(message));

  summary.Set("failedUrl", failed_url_string);
  summary.Set("hostName", host_name);

  result.strings.Set(
      "details", l10n_util::GetStringUTF16(IDS_ERRORPAGE_NET_BUTTON_DETAILS));
  result.strings.Set("hideDetails", l10n_util::GetStringUTF16(
                                        IDS_ERRORPAGE_NET_BUTTON_HIDE_DETAILS));
  result.strings.Set("summary", std::move(summary));

  std::u16string error_code_string;
  if (error_domain == Error::kNetErrorDomain) {
    // Non-internationalized error string, for debugging Chrome itself.
    if (error_code != net::ERR_BLOCKED_BY_ADMINISTRATOR) {
      error_code_string =
          base::ASCIIToUTF16(net::ErrorToShortString(error_code));
    }
  } else if (error_domain == Error::kHttpErrorDomain) {
    error_code_string = base::ASCIIToUTF16(HttpErrorCodeToString(error_code));
  } else if (error_domain == Error::kDnsProbeErrorDomain) {
    error_code_string =
        base::ASCIIToUTF16(error_page::DnsProbeStatusToString(error_code));
  } else if (error_domain == Error::kLinkPreviewErrorDomain) {
    // NOP. Link Preview doesn't show error code and describes an error with
    // text only.
  } else {
    NOTREACHED();
  }
  result.strings.Set("errorCode", error_code_string);

  base::Value::List suggestions_details;
  base::Value::List suggestions_summary_list;

  // Add the reload suggestion, if needed, for pages that didn't come
  // from a post.
  if ((options.buttons & SHOW_BUTTON_RELOAD) && !is_post) {
    base::Value::Dict reload_button;
    result.reload_button_shown = true;
    reload_button.Set("msg",
                      l10n_util::GetStringUTF16(IDS_ERRORPAGES_BUTTON_RELOAD));
    reload_button.Set("reloadUrl", failed_url.spec());
    result.strings.Set("reloadButton", std::move(reload_button));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeOS has its own diagnostics extension, which doesn't rely on a
  // browser-initiated dialog.
  can_show_network_diagnostics_dialog = true;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Add default suggestions and any relevant supporting details.
  GetSuggestionsSummaryList(error_code, result.strings, options.suggestions,
                            locale, suggestions_summary_list,
                            can_show_network_diagnostics_dialog, failed_url,
                            error_page_params);
  AddSuggestionsDetails(error_code, options.suggestions, suggestions_details);

#if BUILDFLAG(IS_ANDROID)
  if (!is_post && !result.reload_button_shown && !is_incognito &&
      failed_url.is_valid() && failed_url.SchemeIsHTTPOrHTTPS() &&
      LocalizedError::IsOfflineError(error_domain, error_code)) {
    if (!auto_fetch_feature_enabled) {
      result.download_button_shown = true;
      result.strings.SetByDottedPath(
          "downloadButton.msg",
          l10n_util::GetStringUTF16(IDS_ERRORPAGES_BUTTON_DOWNLOAD));
      result.strings.SetByDottedPath(
          "downloadButton.disabledMsg",
          l10n_util::GetStringUTF16(IDS_ERRORPAGES_BUTTON_DOWNLOADING));
    } else {
      result.auto_fetch_allowed = true;
      result.strings.Set("attemptAutoFetch", "true");
      result.strings.SetByDottedPath(
          "savePageLater.savePageMsg",
          l10n_util::GetStringUTF16(IDS_ERRORPAGES_SAVE_PAGE_BUTTON));
      result.strings.SetByDottedPath(
          "savePageLater.cancelMsg",
          l10n_util::GetStringUTF16(IDS_ERRORPAGES_CANCEL_SAVE_PAGE_BUTTON));
    }
  }

  result.strings.Set(
      "closeDescriptionPopup",
      l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUGGESTION_CLOSE_POPUP_BUTTON));

  if (LocalizedError::IsOfflineError(error_domain, error_code) &&
      !is_incognito) {
    result.offline_content_feature_enabled = offline_content_feature_enabled;
    if (offline_content_feature_enabled) {
      result.strings.Set("suggestedOfflineContentPresentation", "on");
      result.strings.SetByDottedPath(
          "offlineContentList.title",
          l10n_util::GetStringUTF16(IDS_ERRORPAGES_OFFLINE_CONTENT_LIST_TITLE));
      result.strings.SetByDottedPath(
          "offlineContentList.actionText",
          l10n_util::GetStringUTF16(
              IDS_ERRORPAGES_OFFLINE_CONTENT_LIST_OPEN_ALL_BUTTON));
      result.strings.SetByDottedPath(
          "offlineContentList.showText",
          l10n_util::GetStringUTF16(IDS_SHOW_CONTENT));
      result.strings.SetByDottedPath(
          "offlineContentList.hideText",
          l10n_util::GetStringUTF16(IDS_HIDE_CONTENT));
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)

  result.strings.Set("suggestionsSummaryList",
                     base::Value(std::move(suggestions_summary_list)));
  result.strings.Set("suggestionsDetails",
                     base::Value(std::move(suggestions_details)));
  return result;
}

LocalizedError::PageState LocalizedError::GetPageStateForOverriddenErrorPage(
    base::Value::Dict string_dict,
    int error_code,
    const std::string& error_domain,
    const GURL& failed_url,
    const std::string& locale) {
  LocalizedError::PageState result;

  result.strings.Merge(std::move(string_dict));
  webui::SetLoadTimeDataDefaults(locale, &result.strings);

  if (failed_url.SchemeIsHTTPOrHTTPS()) {
    result.strings.Set("title", url_formatter::IDNToUnicode(failed_url.host()));
  } else {
    std::u16string failed_url_string(url_formatter::FormatUrl(
        failed_url, url_formatter::kFormatUrlOmitNothing,
        base::UnescapeRule::NORMAL, nullptr, nullptr, nullptr));
    // URLs are always LTR.
    if (base::i18n::IsRTL())
      base::i18n::WrapStringWithLTRFormatting(&failed_url_string);
    result.strings.Set("title", failed_url_string);
  }

  return result;
}

std::u16string LocalizedError::GetErrorDetails(const std::string& error_domain,
                                               int error_code,
                                               bool is_secure_dns_network_error,
                                               bool is_post) {
  const LocalizedErrorMap* error_map = LookupErrorMap(
      error_domain, error_code, is_secure_dns_network_error, is_post);
  if (error_map)
    return l10n_util::GetStringUTF16(error_map->summary_resource_id);
  else
    return l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE);
}

bool LocalizedError::HasStrings(const std::string& error_domain,
                                int error_code) {
  // Whether or not the there are strings for an error does not depend on
  // whether or not the page was generated by a POST, so just claim it was
  // not. Likewise it does not depend on whether a DNS error resulted from a
  // secure DNS lookup or not.
  return LookupErrorMap(error_domain, error_code,
                        /*is_secure_dns_network_error=*/false,
                        /*is_post=*/false) != nullptr;
}

// Returns true if the error is due to a disconnected network.
bool LocalizedError::IsOfflineError(const std::string& error_domain,
                                    int error_code) {
  return ((error_code == net::ERR_INTERNET_DISCONNECTED &&
           error_domain == Error::kNetErrorDomain) ||
          (error_code == error_page::DNS_PROBE_FINISHED_NO_INTERNET &&
           error_domain == Error::kDnsProbeErrorDomain));
}

}  // namespace error_page
