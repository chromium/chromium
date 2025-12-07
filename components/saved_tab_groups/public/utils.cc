// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/public/utils.h"

#include "base/i18n/rtl.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/data_sharing/public/data_sharing_utils.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"

namespace tab_groups {
namespace {
const char kChromeUINewTabURL[] = "chrome://newtab/";

// Tab title to be shown or synced when tab URL is in an unsupported scheme.
const char* kDefaultTitleOverride = "New Tab";

constexpr char kDebugTabGroupLogEventString[] =
    "%s\n"
    "  Title: %s\n"
    "  ID: %s\n"
    "  Originating ID: %s\n"
    "  Local ID: %s\n"
    "  Collab ID: %s\n"
    "  Hidden: %d\n"
    "  Transition to Shared: %d\n"
    "  Transition to Saved: %d\n"
    "  # Tabs: %d\n";

constexpr char kDebugTabGroupIdLogEventString[] =
    "%s\n"
    "  ID: %s\n"
    "  Collab ID: %s\n";

// Max number of chars for the title of a URL.
const int kMaxTitleChars = 64;
}  // namespace

bool AreLocalIdsPersisted() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return true;
#else
  return false;
#endif
}

// URL to be synced if an unsupported scheme is navigated in a saved tab group.
const char kChromeSavedTabGroupUnsupportedURL[] =
    "chrome://saved-tab-groups-unsupported";

std::string LocalTabGroupIDToString(const LocalTabGroupID& local_tab_group_id) {
  return local_tab_group_id.ToString();
}

std::optional<LocalTabGroupID> LocalTabGroupIDFromString(
    const std::string& serialized_local_tab_group_id) {
#if BUILDFLAG(IS_ANDROID)
  return base::Token::FromString(serialized_local_tab_group_id);
#else
  auto token = base::Token::FromString(serialized_local_tab_group_id);
  if (!token.has_value()) {
    return std::nullopt;
  }

  return tab_groups::TabGroupId::FromRawToken(token.value());
#endif
}

bool IsURLValidForSavedTabGroups(const GURL& gurl) {
  if (data_sharing::features::IsDataSharingFunctionalityEnabled() &&
      data_sharing::DataSharingUtils::ShouldInterceptNavigationForShareURL(
          gurl)) {
    return false;
  }
  return gurl.SchemeIsHTTPOrHTTPS() || gurl == GURL(kChromeUINewTabURL);
}

std::pair<GURL, std::u16string> GetDefaultUrlAndTitle() {
  return std::make_pair(GURL(kChromeUINewTabURL),
                        base::ASCIIToUTF16(kDefaultTitleOverride));
}

std::u16string GetTitleFromUrlForDisplay(const GURL& url) {
  std::u16string title = url_formatter::FormatUrl(
      url,
      url_formatter::kFormatUrlOmitDefaults |
          url_formatter::kFormatUrlOmitTrivialSubdomains |
          url_formatter::kFormatUrlOmitHTTPS,
      base::UnescapeRule::SPACES, nullptr, nullptr, nullptr);

  std::u16string display_title;
  if (base::i18n::StringContainsStrongRTLChars(title)) {
    // Wrap the URL in an LTR embedding for proper handling of RTL characters.
    // (RFC 3987 Section 4.1 states that "Bidirectional IRIs MUST be rendered in
    // the same way as they would be if they were in a left-to-right
    // embedding".)
    base::i18n::WrapStringWithLTRFormatting(&title);
  }

  // The tab title for display is not a security surface, so we can do the easy
  // elision from the trailing end instead of more complicated elision, or
  // restricting to only showing the hostname.
  gfx::ElideString(title, kMaxTitleChars, &display_title);
  return display_title;
}

std::string TabGroupToShortLogString(const std::string_view& prefix,
                                     const SavedTabGroup* group) {
  if (!group) {
    return "[Null]";
  }

  std::string local_id_str = group->local_group_id().has_value()
                                 ? group->local_group_id()->ToString()
                                 : "N/A";
  std::string collab_id_str = group->collaboration_id().has_value()
                                  ? group->collaboration_id()->value()
                                  : "N/A";
  std::string originating_id_str =
      group->GetOriginatingTabGroupGuid().has_value()
          ? group->GetOriginatingTabGroupGuid()->AsLowercaseString()
          : "N/A";

  std::string display_title = base::UTF16ToUTF8(group->title());

  std::string log = base::StringPrintf(
      kDebugTabGroupLogEventString, prefix, display_title,
      group->saved_guid().AsLowercaseString(), originating_id_str, local_id_str,
      collab_id_str, group->is_hidden(), group->is_transitioning_to_shared(),
      group->is_transitioning_to_saved(), group->saved_tabs().size());
  return log;
}

std::string TabGroupIdsToShortLogString(
    const std::string_view& prefix,
    base::Uuid group_id,
    const std::optional<syncer::CollaborationId> collaboration_id) {
  std::string collab_id_str =
      collaboration_id.has_value() ? collaboration_id->value() : "N/A";

  std::string log =
      base::StringPrintf(kDebugTabGroupIdLogEventString, prefix,
                         group_id.AsLowercaseString(), collab_id_str);
  return log;
}

}  // namespace tab_groups
