// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/error_page/common/localized_error.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/error_page/common/error.h"
#include "components/error_page/common/error_page_params.h"
#include "components/error_page/common/error_page_switches.h"
#include "components/error_page/common/net_error_info.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/origin.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace error_page {

namespace {

static const char kRedirectLoopLearnMoreUrl[] =
    "https://support.google.com/chrome?p=rl_error";
static const char kWeakDHKeyLearnMoreUrl[] =
    "https://support.google.com/chrome?p=dh_error";
static const int kGoogleCachedCopySuggestionType = 0;

enum NAV_SUGGESTIONS {
  SUGGEST_NONE                              = 0,
  SUGGEST_DIAGNOSE_TOOL                     = 1 << 0,
  SUGGEST_CHECK_CONNECTION                  = 1 << 1,
  SUGGEST_DNS_CONFIG                        = 1 << 2,
  SUGGEST_FIREWALL_CONFIG                   = 1 << 3,
  SUGGEST_PROXY_CONFIG                      = 1 << 4,
  SUGGEST_DISABLE_EXTENSION                 = 1 << 5,
  SUGGEST_LEARNMORE                         = 1 << 6,
  SUGGEST_CONTACT_ADMINISTRATOR             = 1 << 7,
  SUGGEST_UNSUPPORTED_CIPHER                = 1 << 8,
  SUGGEST_ANTIVIRUS_CONFIG                  = 1 << 9,
  SUGGEST_OFFLINE_CHECKS                    = 1 << 10,
  SUGGEST_COMPLETE_SETUP                    = 1 << 11,
  // Reload page suggestion for pages created by a post.
  SUGGEST_REPOST_RELOAD                     = 1 << 12,
  SUGGEST_NAVIGATE_TO_ORIGIN                = 1 << 13,
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
       SUGGEST_DIAGNOSE_TOOL,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_CONNECTION_TIMED_OUT,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_TIMED_OUT,
   SUGGEST_CHECK_CONNECTION | SUGGEST_FIREWALL_CONFIG | SUGGEST_PROXY_CONFIG |
       SUGGEST_DIAGNOSE_TOOL,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_CONNECTION_CLOSED,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_CONNECTION_CLOSED,
   SUGGEST_CHECK_CONNECTION | SUGGEST_FIREWALL_CONFIG | SUGGEST_PROXY_CONFIG |
       SUGGEST_DIAGNOSE_TOOL,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_CONNECTION_RESET,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_CONNECTION_RESET,
   SUGGEST_CHECK_CONNECTION | SUGGEST_FIREWALL_CONFIG | SUGGEST_PROXY_CONFIG |
       SUGGEST_DIAGNOSE_TOOL,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_CONNECTION_REFUSED,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_CONNECTION_REFUSED,
   SUGGEST_CHECK_CONNECTION | SUGGEST_FIREWALL_CONFIG | SUGGEST_PROXY_CONFIG,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_CONNECTION_FAILED,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_CONNECTION_FAILED,
   SUGGEST_DIAGNOSE_TOOL,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_NAME_NOT_RESOLVED,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NAME_NOT_RESOLVED,
   SUGGEST_CHECK_CONNECTION | SUGGEST_DNS_CONFIG | SUGGEST_FIREWALL_CONFIG |
       SUGGEST_PROXY_CONFIG | SUGGEST_DIAGNOSE_TOOL,
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
   SUGGEST_DIAGNOSE_TOOL,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_NETWORK_ACCESS_DENIED,
   IDS_ERRORPAGES_HEADING_NETWORK_ACCESS_DENIED,
   IDS_ERRORPAGES_SUMMARY_NETWORK_ACCESS_DENIED,
   SUGGEST_CHECK_CONNECTION | SUGGEST_FIREWALL_CONFIG |
       SUGGEST_ANTIVIRUS_CONFIG | SUGGEST_DIAGNOSE_TOOL,
   SHOW_NO_BUTTONS,
  },
  {net::ERR_PROXY_CONNECTION_FAILED,
   IDS_ERRORPAGES_HEADING_INTERNET_DISCONNECTED,
   IDS_ERRORPAGES_SUMMARY_PROXY_CONNECTION_FAILED,
   SUGGEST_PROXY_CONFIG | SUGGEST_CONTACT_ADMINISTRATOR | SUGGEST_DIAGNOSE_TOOL,
   SHOW_NO_BUTTONS,
  },
  {net::ERR_INTERNET_DISCONNECTED,
   IDS_ERRORPAGES_HEADING_INTERNET_DISCONNECTED,
   IDS_ERRORPAGES_HEADING_INTERNET_DISCONNECTED,
   SUGGEST_OFFLINE_CHECKS | SUGGEST_DIAGNOSE_TOOL,
   SHOW_NO_BUTTONS,
  },
  {net::ERR_FILE_NOT_FOUND,
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
   IDS_ERRORPAGES_HEADING_INSECURE_CONNECTION,
   IDS_ERRORPAGES_SUMMARY_BAD_SSL_CLIENT_AUTH_CERT,
   SUGGEST_CONTACT_ADMINISTRATOR,
   SHOW_NO_BUTTONS,
  },
  {net::ERR_SSL_VERSION_INTERFERENCE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_CONNECTION_FAILED,
   SUGGEST_CHECK_CONNECTION | SUGGEST_FIREWALL_CONFIG | SUGGEST_PROXY_CONFIG,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_TLS13_DOWNGRADE_DETECTED,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_CONNECTION_FAILED,
   SUGGEST_CHECK_CONNECTION | SUGGEST_FIREWALL_CONFIG | SUGGEST_PROXY_CONFIG,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_SSL_WEAK_SERVER_EPHEMERAL_DH_KEY,
   IDS_ERRORPAGES_HEADING_INSECURE_CONNECTION,
   IDS_ERRORPAGES_SUMMARY_SSL_SECURITY_ERROR,
   SUGGEST_LEARNMORE,
   SHOW_NO_BUTTONS,
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
   IDS_ERRORPAGES_SUMMARY_BLOCKED_BY_EXTENSION,
   SUGGEST_DISABLE_EXTENSION,
   SHOW_BUTTON_RELOAD,
  },
  {net::ERR_BLOCKED_BY_XSS_AUDITOR,
   IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
   IDS_ERRORPAGES_SUMMARY_BLOCKED_BY_XSS_AUDITOR,
   SUGGEST_NAVIGATE_TO_ORIGIN,
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
   SUGGEST_CONTACT_ADMINISTRATOR,
   SHOW_NO_BUTTONS,
  },
  {net::ERR_BLOCKED_ENROLLMENT_CHECK_PENDING,
   IDS_ERRORPAGES_HEADING_INTERNET_DISCONNECTED,
   IDS_ERRORPAGES_SUMMARY_BLOCKED_ENROLLMENT_CHECK_PENDING,
   SUGGEST_COMPLETE_SETUP,
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
  {error_page::DNS_PROBE_POSSIBLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_DNS_PROBE_RUNNING,
   SUGGEST_DIAGNOSE_TOOL,
   SHOW_BUTTON_RELOAD,
  },

  // DNS_PROBE_NOT_RUN is not here; NetErrorHelper will restore the original
  // error, which might be one of several DNS-related errors.

  {error_page::DNS_PROBE_STARTED,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_DNS_PROBE_RUNNING,
   // Include SUGGEST_RELOAD so the More button doesn't jump when we update.
   SUGGEST_DIAGNOSE_TOOL,
   SHOW_BUTTON_RELOAD,
  },

  // DNS_PROBE_FINISHED_UNKNOWN is not here; NetErrorHelper will restore the
  // original error, which might be one of several DNS-related errors.

  {error_page::DNS_PROBE_FINISHED_NO_INTERNET,
   IDS_ERRORPAGES_HEADING_INTERNET_DISCONNECTED,
   IDS_ERRORPAGES_HEADING_INTERNET_DISCONNECTED,
   SUGGEST_OFFLINE_CHECKS | SUGGEST_DIAGNOSE_TOOL,
   SHOW_NO_BUTTONS,
  },
  {error_page::DNS_PROBE_FINISHED_BAD_CONFIG,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NAME_NOT_RESOLVED,
   SUGGEST_DNS_CONFIG | SUGGEST_FIREWALL_CONFIG | SUGGEST_PROXY_CONFIG |
       SUGGEST_DIAGNOSE_TOOL,
   SHOW_BUTTON_RELOAD,
  },
  {error_page::DNS_PROBE_FINISHED_NXDOMAIN,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NAME_NOT_RESOLVED,
   SUGGEST_DIAGNOSE_TOOL,
   SHOW_BUTTON_RELOAD,
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
                                        int error_code, bool is_post) {
  if (error_domain == Error::kNetErrorDomain) {
    // Display a different page in the special case of navigating through the
    // history to an uncached page created by a POST.
    if (is_post && error_code == net::ERR_CACHE_MISS)
      return &repost_error;
    return FindErrorMapInArray(net_error_options,
                               arraysize(net_error_options),
                               error_code);
  } else if (error_domain == Error::kHttpErrorDomain) {
    const LocalizedErrorMap* map = FindErrorMapInArray(
        http_error_options, arraysize(http_error_options), error_code);
    // Handle miscellaneous 400/500 errors.
    return !map && error_code >= 400 && error_code < 600
               ? &generic_4xx_5xx_error
               : map;
  } else if (error_domain == Error::kDnsProbeErrorDomain) {
    const LocalizedErrorMap* map =
        FindErrorMapInArray(dns_probe_error_options,
                            arraysize(dns_probe_error_options),
                            error_code);
    DCHECK(map);
    return map;
  } else {
    NOTREACHED();
    return nullptr;
  }
}

// Returns a dictionary containing the strings for the settings menu under the
// app menu, and the advanced settings button.
base::DictionaryValue* GetStandardMenuItemsText() {
  base::DictionaryValue* standard_menu_items_text = new base::DictionaryValue();
  standard_menu_items_text->SetString("settingsTitle",
      l10n_util::GetStringUTF16(IDS_SETTINGS_TITLE));
  standard_menu_items_text->SetString("advancedTitle",
      l10n_util::GetStringUTF16(IDS_SETTINGS_SHOW_ADVANCED_SETTINGS));
  return standard_menu_items_text;
}

// Returns true if the error is due to a disconnected network.
bool IsOfflineError(const std::string& error_domain, int error_code) {
  return ((error_code == net::ERR_INTERNET_DISCONNECTED &&
           error_domain == Error::kNetErrorDomain) ||
          (error_code == error_page::DNS_PROBE_FINISHED_NO_INTERNET &&
           error_domain == Error::kDnsProbeErrorDomain));
}

// Gets the icon class for a given |error_domain| and |error_code|.
const char* GetIconClassForError(const std::string& error_domain,
                                 int error_code) {
  return IsOfflineError(error_domain, error_code) ? "icon-offline"
                                                  : "icon-generic";
}

// If the first suggestion is for a Google cache copy link. Promote the
// suggestion to a separate set of strings for displaying as a button.
void AddGoogleCachedCopyButton(base::ListValue* suggestions_summary_list,
                               base::DictionaryValue* error_strings) {
  if (!suggestions_summary_list->empty()) {
    base::DictionaryValue* suggestion;
    suggestions_summary_list->GetDictionary(0, &suggestion);
    int type = -1;
    suggestion->GetInteger("type", &type);

    if (type == kGoogleCachedCopySuggestionType) {
      base::string16 cache_url;
      suggestion->GetString("urlCorrection", &cache_url);
      int cache_tracking_id = -1;
      suggestion->GetInteger("trackingId", &cache_tracking_id);
      std::unique_ptr<base::DictionaryValue> cache_button(
          new base::DictionaryValue);
      cache_button->SetString(
            "msg",
            l10n_util::GetStringUTF16(IDS_ERRORPAGES_BUTTON_SHOW_SAVED_COPY));
      cache_button->SetString("cacheUrl", cache_url);
      cache_button->SetInteger("trackingId", cache_tracking_id);
      error_strings->Set("cacheButton", std::move(cache_button));

      // Remove the item from suggestions dictionary so that it does not get
      // displayed by the template in the details section.
      suggestions_summary_list->Remove(0, nullptr);
    }
  }
}

// Helper function that creates a single entry dictionary and adds it
// to a ListValue,
void AddSingleEntryDictionaryToList(base::ListValue* list,
                                    const char* path,
                                    int message_id,
                                    bool insert_as_first_item) {
  auto suggestion_list_item = std::make_unique<base::DictionaryValue>();
  suggestion_list_item->SetString(path, l10n_util::GetStringUTF16(message_id));

  if (insert_as_first_item) {
    list->Insert(0, std::move(suggestion_list_item));
  } else {
    list->Append(std::move(suggestion_list_item));
  }
}

// Adds a linked suggestion dictionary entry to the suggestions list.
void AddLinkedSuggestionToList(const int error_code,
                               const std::string& locale,
                               base::ListValue* suggestions_summary_list,
                               bool standalone_suggestion) {
  GURL learn_more_url;
  base::string16 suggestion_string = standalone_suggestion ?
      l10n_util::GetStringUTF16(
          IDS_ERRORPAGES_SUGGESTION_LEARNMORE_SUMMARY_STANDALONE) :
      l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUGGESTION_LEARNMORE_SUMMARY);

  switch (error_code) {
    case net::ERR_SSL_WEAK_SERVER_EPHEMERAL_DH_KEY:
      learn_more_url = GURL(kWeakDHKeyLearnMoreUrl);
      break;
    case net::ERR_TOO_MANY_REDIRECTS:
      learn_more_url = GURL(kRedirectLoopLearnMoreUrl);
      suggestion_string = l10n_util::GetStringUTF16(
          IDS_ERRORPAGES_SUGGESTION_CLEAR_COOKIES_SUMMARY);
      break;
    default:
      NOTREACHED();
      break;
  }

  DCHECK(learn_more_url.is_valid());
  // Add the language parameter to the URL.
  std::string query = learn_more_url.query() + "&hl=" + locale;
  GURL::Replacements repl;
  repl.SetQueryStr(query);
  GURL learn_more_url_with_locale = learn_more_url.ReplaceComponents(repl);

  std::unique_ptr<base::DictionaryValue> suggestion_list_item(
      new base::DictionaryValue);
  suggestion_list_item->SetString("summary", suggestion_string);
  suggestion_list_item->SetString("learnMoreUrl",
      learn_more_url_with_locale.spec());
  suggestions_summary_list->Append(std::move(suggestion_list_item));
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
                               base::DictionaryValue* error_strings,
                               int suggestions,
                               const std::string& locale,
                               base::ListValue* suggestions_summary_list,
                               bool can_show_network_diagnostics_dialog,
                               const GURL& failed_url) {
  // Remove the diagnostic tool suggestion if the platform doesn't support it
  // or the url isn't valid.
  if (!can_show_network_diagnostics_dialog || !failed_url.is_valid() ||
      !failed_url.SchemeIsHTTPOrHTTPS()) {
    suggestions &= ~SUGGEST_DIAGNOSE_TOOL;
  }

  if (suggestions == SUGGEST_NONE)
    return;

  if (IsOnlySuggestion(suggestions, SUGGEST_CONTACT_ADMINISTRATOR)) {
    DCHECK(suggestions_summary_list->empty());
    DCHECK(!(suggestions & ~SUGGEST_CONTACT_ADMINISTRATOR));
    AddSingleEntryDictionaryToList(suggestions_summary_list, "summary",
        IDS_ERRORPAGES_SUGGESTION_CONTACT_ADMIN_SUMMARY_STANDALONE, false);
    return;
  }
  if (IsSuggested(suggestions, SUGGEST_CONTACT_ADMINISTRATOR)) {
    AddSingleEntryDictionaryToList(suggestions_summary_list, "summary",
        IDS_ERRORPAGES_SUGGESTION_CONTACT_ADMIN_SUMMARY, false);
  }

  if (IsOnlySuggestion(suggestions, SUGGEST_COMPLETE_SETUP)) {
    DCHECK(suggestions_summary_list->empty());
    DCHECK(!(suggestions & ~SUGGEST_COMPLETE_SETUP));
    AddSingleEntryDictionaryToList(suggestions_summary_list, "summary",
        IDS_ERRORPAGES_SUGGESTION_DIAGNOSE_CONNECTION_SUMMARY, false);
    AddSingleEntryDictionaryToList(suggestions_summary_list, "summary",
        IDS_ERRORPAGES_SUGGESTION_COMPLETE_SETUP_SUMMARY, false);
    return;
  }
  DCHECK(!IsSuggested(suggestions, SUGGEST_COMPLETE_SETUP));

  if (IsOnlySuggestion(suggestions,SUGGEST_REPOST_RELOAD)) {
    DCHECK(suggestions_summary_list->empty());
    DCHECK(!(suggestions & ~SUGGEST_REPOST_RELOAD));
    // If the page was created by a post, it can't be reloaded in the same
    // way, so just add a suggestion instead.
    // TODO(mmenke):  Make the reload button bring up the repost confirmation
    //                dialog for pages resulting from posts.
    AddSingleEntryDictionaryToList(suggestions_summary_list, "summary",
        IDS_ERRORPAGES_SUGGESTION_RELOAD_REPOST_SUMMARY, false);
    return;
  }
  DCHECK(!IsSuggested(suggestions, SUGGEST_REPOST_RELOAD));

  if (IsOnlySuggestion(suggestions, SUGGEST_NAVIGATE_TO_ORIGIN)) {
    DCHECK(suggestions_summary_list->empty());
    DCHECK(!(suggestions & ~SUGGEST_NAVIGATE_TO_ORIGIN));
    url::Origin failed_origin = url::Origin::Create(failed_url);
    if (failed_origin.opaque())
      return;

    auto suggestion = std::make_unique<base::DictionaryValue>();
    suggestion->SetString("summary",
                          l10n_util::GetStringUTF16(
                              IDS_ERRORPAGES_SUGGESTION_NAVIGATE_TO_ORIGIN));
    suggestion->SetString("originURL", failed_origin.Serialize());
    suggestions_summary_list->Append(std::move(suggestion));
    return;
  }
  DCHECK(!IsSuggested(suggestions, SUGGEST_NAVIGATE_TO_ORIGIN));

  if (IsOnlySuggestion(suggestions, SUGGEST_LEARNMORE)) {
    DCHECK(suggestions_summary_list->empty());
    AddLinkedSuggestionToList(error_code, locale, suggestions_summary_list,
                              true);
    return;
  }
  if (IsSuggested(suggestions, SUGGEST_LEARNMORE)) {
    AddLinkedSuggestionToList(error_code, locale, suggestions_summary_list,
                              false);
  }

  if (suggestions & SUGGEST_DISABLE_EXTENSION) {
    DCHECK(suggestions_summary_list->empty());
    AddSingleEntryDictionaryToList(suggestions_summary_list, "summary",
        IDS_ERRORPAGES_SUGGESTION_DISABLE_EXTENSION_SUMMARY, false);
    return;
  }
  DCHECK(!IsSuggested(suggestions, SUGGEST_DISABLE_EXTENSION));

  if (suggestions & SUGGEST_CHECK_CONNECTION) {
    AddSingleEntryDictionaryToList(suggestions_summary_list, "summary",
        IDS_ERRORPAGES_SUGGESTION_CHECK_CONNECTION_SUMMARY, false);
  }

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  if (IsSuggested(suggestions, SUGGEST_DNS_CONFIG) &&
      IsSuggested(suggestions, SUGGEST_FIREWALL_CONFIG) &&
      IsSuggested(suggestions, SUGGEST_PROXY_CONFIG)) {
    AddSingleEntryDictionaryToList(suggestions_summary_list, "summary",
        IDS_ERRORPAGES_SUGGESTION_CHECK_PROXY_FIREWALL_DNS_SUMMARY, false);
  } else if (IsSuggested(suggestions, SUGGEST_FIREWALL_CONFIG) &&
             IsSuggested(suggestions, SUGGEST_ANTIVIRUS_CONFIG)) {
    AddSingleEntryDictionaryToList(suggestions_summary_list, "summary",
        IDS_ERRORPAGES_SUGGESTION_CHECK_FIREWALL_ANTIVIRUS_SUMMARY, false);
  } else if (IsSuggested(suggestions, SUGGEST_PROXY_CONFIG) &&
             IsSuggested(suggestions, SUGGEST_FIREWALL_CONFIG)) {
    AddSingleEntryDictionaryToList(suggestions_summary_list, "summary",
        IDS_ERRORPAGES_SUGGESTION_CHECK_PROXY_FIREWALL_SUMMARY, false);
  } else if (IsSuggested(suggestions, SUGGEST_PROXY_CONFIG)) {
    AddSingleEntryDictionaryToList(suggestions_summary_list, "summary",
        IDS_ERRORPAGES_SUGGESTION_CHECK_PROXY_ADDRESS_SUMMARY, false);
  } else {
    DCHECK(!(suggestions & SUGGEST_PROXY_CONFIG));
    DCHECK(!(suggestions & SUGGEST_FIREWALL_CONFIG));
    DCHECK(!(suggestions & SUGGEST_DNS_CONFIG));
  }
#endif

  if (IsSuggested(suggestions, SUGGEST_OFFLINE_CHECKS)) {
#if defined(OS_ANDROID) || defined(OS_IOS)
    AddSingleEntryDictionaryToList(suggestions_summary_list, "summary",
        IDS_ERRORPAGES_SUGGESTION_TURN_OFF_AIRPLANE_SUMMARY, false);
    AddSingleEntryDictionaryToList(suggestions_summary_list, "summary",
        IDS_ERRORPAGES_SUGGESTION_TURN_ON_DATA_SUMMARY, false);
    AddSingleEntryDictionaryToList(suggestions_summary_list, "summary",
        IDS_ERRORPAGES_SUGGESTION_CHECKING_SIGNAL_SUMMARY, false);
#else
    AddSingleEntryDictionaryToList(suggestions_summary_list, "summary",
        IDS_ERRORPAGES_SUGGESTION_CHECK_HARDWARE_SUMMARY, false);
    AddSingleEntryDictionaryToList(suggestions_summary_list, "summary",
        IDS_ERRORPAGES_SUGGESTION_CHECK_WIFI_SUMMARY, false);
#endif
  }

// If the current platform has a directly accesible network diagnostics tool and
// the URL is valid add a suggestion.
#if defined(OS_CHROMEOS) || defined(OS_WIN) || \
    (defined(OS_MACOSX) && !defined(OS_IOS))
  if (IsOnlySuggestion(suggestions, SUGGEST_DIAGNOSE_TOOL)) {
    AddSingleEntryDictionaryToList(suggestions_summary_list, "summary",
        IDS_ERRORPAGES_SUGGESTION_DIAGNOSE_STANDALONE, false);
    return;
  }
  if (IsSuggested(suggestions, SUGGEST_DIAGNOSE_TOOL)) {
    AddSingleEntryDictionaryToList(suggestions_summary_list, "summary",
        IDS_ERRORPAGES_SUGGESTION_DIAGNOSE, false);
  }
#else
  DCHECK(!IsSuggested(suggestions, SUGGEST_DIAGNOSE_TOOL));
#endif  // defined(OS_CHROMEOS) || defined(OS_WIN) ||
        // (defined(OS_MACOSX) && !defined(OS_IOS))

  // Add list prefix header.
  error_strings->SetString("suggestionsSummaryListHeader",
      l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUGGESTION_LIST_HEADER));
}

// Creates a dictionary with "header" and "body" entries and adds it
// to a ListValue, Returns the dictionary to allow further items to be added.
base::DictionaryValue* AddSuggestionDetailDictionaryToList(
    base::ListValue* list,
    int header_message_id,
    int body_message_id,
    bool append_standard_menu_items) {
  base::DictionaryValue* suggestion_list_item;
  if (append_standard_menu_items) {
    suggestion_list_item = GetStandardMenuItemsText();
  } else {
    suggestion_list_item = new base::DictionaryValue;
  }

  if (header_message_id) {
    suggestion_list_item->SetString("header",
        l10n_util::GetStringUTF16(header_message_id));
  }
  if (body_message_id) {
    suggestion_list_item->SetString("body",
        l10n_util::GetStringUTF16(body_message_id));
  }
  list->Append(base::WrapUnique(suggestion_list_item));
  // |suggestion_list_item| is invalidated at this point, so it needs to be
  // reset.
  list->GetDictionary(list->GetSize() - 1, &suggestion_list_item);
  return suggestion_list_item;
}

// Certain suggestions have supporting details which get displayed under
// the "Details" button.
void AddSuggestionsDetails(int error_code,
                           base::DictionaryValue* error_strings,
                           int suggestions,
                           base::ListValue* suggestions_details) {
  if (suggestions & SUGGEST_CHECK_CONNECTION) {
    AddSuggestionDetailDictionaryToList(suggestions_details,
          IDS_ERRORPAGES_SUGGESTION_CHECK_CONNECTION_HEADER,
          IDS_ERRORPAGES_SUGGESTION_CHECK_CONNECTION_BODY, false);
  }

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  if (suggestions & SUGGEST_DNS_CONFIG) {
    AddSuggestionDetailDictionaryToList(suggestions_details,
          IDS_ERRORPAGES_SUGGESTION_DNS_CONFIG_HEADER,
          IDS_ERRORPAGES_SUGGESTION_DNS_CONFIG_BODY, false);

    base::DictionaryValue* suggest_network_prediction =
        AddSuggestionDetailDictionaryToList(suggestions_details,
            IDS_ERRORPAGES_SUGGESTION_NETWORK_PREDICTION_HEADER,
            IDS_ERRORPAGES_SUGGESTION_NETWORK_PREDICTION_BODY, true);

    suggest_network_prediction->SetString(
        "noNetworkPredictionTitle",
        l10n_util::GetStringUTF16(
            IDS_NETWORK_PREDICTION_ENABLED_DESCRIPTION));
  }

  if (suggestions & SUGGEST_FIREWALL_CONFIG) {
    AddSuggestionDetailDictionaryToList(suggestions_details,
        IDS_ERRORPAGES_SUGGESTION_FIREWALL_CONFIG_HEADER,
        IDS_ERRORPAGES_SUGGESTION_FIREWALL_CONFIG_BODY, false);
  }

  if (suggestions & SUGGEST_PROXY_CONFIG) {
    base::DictionaryValue* suggest_proxy_config =
        AddSuggestionDetailDictionaryToList(suggestions_details,
            IDS_ERRORPAGES_SUGGESTION_PROXY_CONFIG_HEADER,
            0, true);

    // Custom body string.
    suggest_proxy_config->SetString("body",
        l10n_util::GetStringFUTF16(IDS_ERRORPAGES_SUGGESTION_PROXY_CONFIG_BODY,
            l10n_util::GetStringUTF16(
                IDS_ERRORPAGES_SUGGESTION_PROXY_DISABLE_PLATFORM)));

    suggest_proxy_config->SetString("proxyTitle",
        l10n_util::GetStringUTF16(IDS_OPTIONS_PROXIES_CONFIGURE_BUTTON));
  }
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
  return std::string("HTTP ERROR ") + base::IntToString(error);
}

}  // namespace

void LocalizedError::GetStrings(
    int error_code,
    const std::string& error_domain,
    const GURL& failed_url,
    bool is_post,
    bool stale_copy_in_cache,
    bool can_show_network_diagnostics_dialog,
    bool is_incognito,
    OfflineContentOnNetErrorFeatureState offline_content_feature_state,
    const std::string& locale,
    std::unique_ptr<error_page::ErrorPageParams> params,
    base::DictionaryValue* error_strings) {
  webui::SetLoadTimeDataDefaults(locale, error_strings);

  // Grab the strings and settings that depend on the error type.  Init
  // options with default values.
  LocalizedErrorMap options = {
    0,
    IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
    IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE,
    SUGGEST_NONE,
    SHOW_NO_BUTTONS,
  };

  const LocalizedErrorMap* error_map = LookupErrorMap(error_domain, error_code,
                                                      is_post);
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

  base::string16 failed_url_string(url_formatter::FormatUrl(
      failed_url, url_formatter::kFormatUrlOmitNothing,
      net::UnescapeRule::NORMAL, nullptr, nullptr, nullptr));
  // URLs are always LTR.
  if (base::i18n::IsRTL())
    base::i18n::WrapStringWithLTRFormatting(&failed_url_string);

  base::string16 host_name(url_formatter::IDNToUnicode(failed_url.host()));
  if (failed_url.SchemeIsHTTPOrHTTPS())
    error_strings->SetString("title", host_name);
  else
    error_strings->SetString("title", failed_url_string);

  std::string icon_class = GetIconClassForError(error_domain, error_code);
  error_strings->SetString("iconClass", icon_class);

  auto heading = std::make_unique<base::DictionaryValue>();
  heading->SetString("msg",
                     l10n_util::GetStringUTF16(options.heading_resource_id));
  heading->SetString("hostName", host_name);
  error_strings->Set("heading", std::move(heading));

  auto summary = std::make_unique<base::DictionaryValue>();

  // Set summary message under the heading.
  summary->SetString(
      "msg", l10n_util::GetStringUTF16(options.summary_resource_id));

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Check if easter egg should be disabled.
  if (command_line->HasSwitch(
          error_page::switches::kDisableDinosaurEasterEgg)) {
    // The presence of this string disables the easter egg. Acts as a flag.
    error_strings->SetString("disabledEasterEgg",
        l10n_util::GetStringUTF16(IDS_ERRORPAGE_FUN_DISABLED));
  }

  summary->SetString("failedUrl", failed_url_string);
  summary->SetString("hostName", host_name);

  error_strings->SetString(
      "details", l10n_util::GetStringUTF16(IDS_ERRORPAGE_NET_BUTTON_DETAILS));
  error_strings->SetString(
      "hideDetails", l10n_util::GetStringUTF16(
          IDS_ERRORPAGE_NET_BUTTON_HIDE_DETAILS));
  error_strings->Set("summary", std::move(summary));

  base::string16 error_string;
  if (error_domain == Error::kNetErrorDomain) {
    // Non-internationalized error string, for debugging Chrome itself.
    error_string = base::ASCIIToUTF16(net::ErrorToShortString(error_code));
  } else if (error_domain == Error::kDnsProbeErrorDomain) {
    std::string ascii_error_string =
        error_page::DnsProbeStatusToString(error_code);
    error_string = base::ASCIIToUTF16(ascii_error_string);
  } else {
    DCHECK_EQ(Error::kHttpErrorDomain, error_domain);
    error_string = base::ASCIIToUTF16(HttpErrorCodeToString(error_code));
  }
  error_strings->SetString("errorCode", error_string);

  // If no parameters were provided, use the defaults.
  if (!params) {
    params.reset(new error_page::ErrorPageParams());
    params->suggest_reload = !!(options.buttons & SHOW_BUTTON_RELOAD);
  }

  base::ListValue* suggestions_details = nullptr;
  base::ListValue* suggestions_summary_list = nullptr;

  bool use_default_suggestions = true;
  if (!params->override_suggestions) {
    // Detailed suggestion information.
    suggestions_details = error_strings->SetList(
        "suggestionsDetails", std::make_unique<base::ListValue>());
    suggestions_summary_list = error_strings->SetList(
        "suggestionsSummaryList", std::make_unique<base::ListValue>());
  } else {
    suggestions_summary_list = error_strings->SetList(
        "suggestionsSummaryList", std::move(params->override_suggestions));
    use_default_suggestions = false;
    AddGoogleCachedCopyButton(suggestions_summary_list, error_strings);
  }

  if (params->search_url.is_valid()) {
    std::unique_ptr<base::DictionaryValue> search_suggestion(
        new base::DictionaryValue);
    search_suggestion->SetString("summary",l10n_util::GetStringUTF16(
        IDS_ERRORPAGES_SUGGESTION_GOOGLE_SEARCH_SUMMARY));
    search_suggestion->SetString("searchUrl", params->search_url.spec() +
                                 params->search_terms);
    search_suggestion->SetString("searchTerms", params->search_terms);
    search_suggestion->SetInteger("trackingId",
                                  params->search_tracking_id);
    suggestions_summary_list->Append(std::move(search_suggestion));
  }

  // Add the reload suggestion, if needed for pages that didn't come
  // from a post.
#if defined(OS_ANDROID)
  bool reload_visible = false;
#endif  // defined(OS_ANDROID)
  if (params->suggest_reload && !is_post) {
#if defined(OS_ANDROID)
    reload_visible = true;
#endif  // defined(OS_ANDROID)
    auto reload_button = std::make_unique<base::DictionaryValue>();
    reload_button->SetString(
        "msg", l10n_util::GetStringUTF16(IDS_ERRORPAGES_BUTTON_RELOAD));
    reload_button->SetString("reloadUrl", failed_url.spec());
    reload_button->SetInteger("reloadTrackingId", params->reload_tracking_id);
    error_strings->Set("reloadButton", std::move(reload_button));
  }

  // If not using the default suggestions, nothing else to do.
  if (!use_default_suggestions)
    return;

#if defined(OS_CHROMEOS)
  // ChromeOS has its own diagnostics extension, which doesn't rely on a
  // browser-initiated dialog.
  can_show_network_diagnostics_dialog = true;
#endif  // defined(OS_CHROMEOS)

  // Add default suggestions and any relevant supporting details.
  GetSuggestionsSummaryList(error_code, error_strings, options.suggestions,
                            locale, suggestions_summary_list,
                            can_show_network_diagnostics_dialog, failed_url);
  AddSuggestionsDetails(error_code, error_strings, options.suggestions,
                        suggestions_details);

  // Add action buttons.
  const std::string& show_saved_copy_value =
      command_line->GetSwitchValueASCII(error_page::switches::kShowSavedCopy);
  bool show_saved_copy_primary =
      (show_saved_copy_value ==
       error_page::switches::kEnableShowSavedCopyPrimary);
  bool show_saved_copy_secondary =
      (show_saved_copy_value ==
       error_page::switches::kEnableShowSavedCopySecondary);
  bool show_saved_copy_visible =
      (stale_copy_in_cache && !is_post &&
      (show_saved_copy_primary || show_saved_copy_secondary));

  if (show_saved_copy_visible) {
    auto show_saved_copy_button = std::make_unique<base::DictionaryValue>();
    show_saved_copy_button->SetString(
        "msg", l10n_util::GetStringUTF16(
            IDS_ERRORPAGES_BUTTON_SHOW_SAVED_COPY));
    show_saved_copy_button->SetString(
        "title",
        l10n_util::GetStringUTF16(IDS_ERRORPAGES_BUTTON_SHOW_SAVED_COPY_HELP));
    if (show_saved_copy_primary)
      show_saved_copy_button->SetString("primary", "true");
    error_strings->Set("showSavedCopyButton",
                       std::move(show_saved_copy_button));
  }

#if defined(OS_ANDROID)
  if (!is_post && !reload_visible && !show_saved_copy_visible &&
      !is_incognito && failed_url.is_valid() &&
      failed_url.SchemeIsHTTPOrHTTPS() &&
      IsOfflineError(error_domain, error_code)) {
    error_strings->SetPath(
        {"downloadButton", "msg"},
        base::Value(l10n_util::GetStringUTF16(IDS_ERRORPAGES_BUTTON_DOWNLOAD)));
    error_strings->SetPath({"downloadButton", "disabledMsg"},
                           base::Value(l10n_util::GetStringUTF16(
                               IDS_ERRORPAGES_BUTTON_DOWNLOADING)));
  }

  error_strings->SetString(
      "closeDescriptionPopup",
      l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUGGESTION_CLOSE_POPUP_BUTTON));

  if (IsOfflineError(error_domain, error_code) && !is_incognito) {
    switch (offline_content_feature_state) {
      case OfflineContentOnNetErrorFeatureState::kDisabled:
        break;
      case OfflineContentOnNetErrorFeatureState::kEnabledList:
        error_strings->SetString("suggestedOfflineContentPresentationMode",
                                 "list");
        error_strings->SetPath({"offlineContentList", "title"},
                               base::Value(l10n_util::GetStringUTF16(
                                   IDS_ERRORPAGES_OFFLINE_CONTENT_LIST_TITLE)));
        error_strings->SetPath(
            {"offlineContentList", "actionText"},
            base::Value(l10n_util::GetStringUTF16(
                IDS_ERRORPAGES_OFFLINE_CONTENT_LIST_OPEN_ALL_BUTTON)));
        break;
      case OfflineContentOnNetErrorFeatureState::kEnabledSummary:
        error_strings->SetString("suggestedOfflineContentPresentationMode",
                                 "summary");
        error_strings->SetPath(
            {"offlineContentSummary", "description"},
            base::Value(l10n_util::GetStringUTF16(
                IDS_ERRORPAGES_OFFLINE_CONTENT_SUMMARY_DESCRIPTION)));
        error_strings->SetPath(
            {"offlineContentSummary", "actionText"},
            base::Value(l10n_util::GetStringUTF16(
                IDS_ERRORPAGES_OFFLINE_CONTENT_SUMMARY_BUTTON)));
        break;
    }
  }
#endif  // defined(OS_ANDROID)
}

base::string16 LocalizedError::GetErrorDetails(const std::string& error_domain,
                                               int error_code,
                                               bool is_post) {
  const LocalizedErrorMap* error_map =
      LookupErrorMap(error_domain, error_code, is_post);
  if (error_map)
    return l10n_util::GetStringUTF16(error_map->summary_resource_id);
  else
    return l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE);
}

bool LocalizedError::HasStrings(const std::string& error_domain,
                                int error_code) {
  // Whether or not the there are strings for an error does not depend on
  // whether or not the page was be generated by a POST, so just claim it was
  // not.
  return LookupErrorMap(error_domain, error_code, /*is_post=*/false) != nullptr;
}

}  // namespace error_page
