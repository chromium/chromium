// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/hovercard/hover_card_anchor_target.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"
#include "chrome/browser/ui/tabs/tab_data.h"
#include "chrome/browser/ui/tabs/tab_group_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "chrome/browser/ui/views/tabs/hovercard/filename_elider.h"
#include "chrome/grit/generated_resources.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_group.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

namespace {

FadeLabelViewData GetTabTitleLabel(const tabs::TabGroupTabData& tab_data) {
  std::u16string title;
  GURL domain_url;

  // Use committed URL to determine if no page has yet loaded, since the title
  // can be blank for some web pages.
  if (!tab_data.last_committed_url.is_valid()) {
    domain_url = tab_data.visible_url;
    title = tab_data.is_crashed
                ? l10n_util::GetStringUTF16(IDS_HOVER_CARD_CRASHED_TITLE)
                : l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE);
  } else {
    domain_url = tab_data.last_committed_url;
    title = tab_data.title;
  }

  bool is_filename = false;
  if (domain_url.SchemeIsFile()) {
    is_filename = true;
  } else {
    // Most of the time we want our standard (tail-elided) formatting for web
    // pages, but when viewing an image in the browser, many users want to
    // view the image dimensions (see crbug.com/1222984) so for titles that
    // "look" like images (i.e. that end with a dimension) we instead switch
    // to middle-elide.
    if (FilenameElider::FindImageDimensions(title) != std::u16string::npos) {
      is_filename = true;
    }
  }

  return {title, is_filename};
}

FadeLabelViewData GetTabTitleLabel(const tabs::TabData& tab_data) {
  tabs::TabGroupTabData tab_group_tab_data;
  tab_group_tab_data.last_committed_url = tab_data.last_committed_url;
  tab_group_tab_data.visible_url = tab_data.visible_url;
  tab_group_tab_data.title = tab_data.title;
  tab_group_tab_data.is_crashed = tab_data.is_crashed;
  tab_group_tab_data.should_display_url = tab_data.should_display_url;
  return GetTabTitleLabel(tab_group_tab_data);
}
}  // namespace

TabCardData::TabCardData() = default;
TabCardData::~TabCardData() = default;
GroupCardData::GroupCardData() = default;
GroupCardData::~GroupCardData() = default;

HoverCardAnchorTarget::HoverCardAnchorTarget(views::View* anchor_view)
    : anchor_view_(anchor_view) {
  CHECK(anchor_view_);
}

HoverCardAnchorTarget::~HoverCardAnchorTarget() = default;

void HoverCardAnchorTarget::SetHoverCardDataFrom(
    const tabs::TabData& tab_data) {
  hover_card_data_.emplace<TabCardData>();
  GURL domain_url;
  std::optional<tabs::TabAlert> alert_state;

  // Use committed URL to determine if no page has yet loaded, since the title
  // can be blank for some web pages.
  if (!tab_data.last_committed_url.is_valid()) {
    domain_url = tab_data.visible_url;
    alert_state = std::nullopt;
  } else {
    domain_url = tab_data.last_committed_url;
    alert_state = tab_data.alert_state;
  }

  std::u16string domain;
  bool is_domain_url = false;
  if (domain_url.SchemeIsFile()) {
    domain = l10n_util::GetStringUTF16(IDS_HOVER_CARD_FILE_URL_SOURCE);
  } else if (domain_url.SchemeIsBlob()) {
    domain = l10n_util::GetStringUTF16(IDS_HOVER_CARD_BLOB_URL_SOURCE);
  } else if (domain_url.SchemeIs(url::kViewSourceScheme)) {
    domain = l10n_util::GetStringUTF16(IDS_HOVER_CARD_VIEW_SOURCE_URL_SOURCE);
  } else {
    if (tab_data.should_display_url) {
      is_domain_url = true;
      // Hide the domain when necessary. This leaves an empty space in the
      // card, but this scenario is very rare. Also, shrinking the card to
      // remove the space would result in visual noise, so we keep it simple.
      domain = url_formatter::FormatUrl(
          domain_url,
          url_formatter::kFormatUrlOmitDefaults |
              url_formatter::kFormatUrlOmitHTTPS |
              url_formatter::kFormatUrlOmitTrivialSubdomains |
              url_formatter::kFormatUrlTrimAfterHost,
          base::UnescapeRule::NORMAL, nullptr, nullptr, nullptr);
    }
  }

  TabCardData& card_data = std::get<TabCardData>(hover_card_data_);

  card_data.thumbnail = tab_data.thumbnail;
  card_data.title_data = GetTabTitleLabel(tab_data);
  card_data.domain_data = {domain, false, gfx::ELIDE_HEAD, is_domain_url};

  // Now set the collaboration data.
  using collaboration::messaging::CollaborationEvent;
  card_data.show_collaboration_messaging = false;

  if (tab_data.collaboration_messaging &&
      tab_data.collaboration_messaging->HasMessage()) {
    card_data.show_collaboration_messaging = true;
    switch (tab_data.collaboration_messaging->collaboration_event()) {
      case CollaborationEvent::TAB_ADDED:
        card_data.collaboration_message = l10n_util::GetStringFUTF16(
            IDS_DATA_SHARING_RECENT_ACTIVITY_MEMBER_ADDED_THIS_TAB,
            tab_data.collaboration_messaging->given_name());
        break;
      case CollaborationEvent::TAB_UPDATED:
        card_data.collaboration_message = l10n_util::GetStringFUTF16(
            IDS_DATA_SHARING_RECENT_ACTIVITY_MEMBER_CHANGED_THIS_TAB,
            tab_data.collaboration_messaging->given_name());
        break;
      default:
        NOTREACHED();
    }
  }

  if (tab_data.collaboration_messaging) {
    card_data.collaboration_avatar = tab_data.collaboration_messaging->avatar();
  }

  base::ByteSize memory_savings =
      tab_data.discarded_memory_savings.has_value()
          ? tab_data.discarded_memory_savings.value()
          : base::ByteSize(0);

  card_data.alert_data = {alert_state, tab_data.should_show_discard_status,
                          memory_savings};
  card_data.tab_resource_usage = tab_data.tab_resource_usage;
  card_data.show_discard_status = tab_data.should_show_discard_status;
  card_data.is_tab_discarded = tab_data.is_tab_discarded;
  card_data.is_crashed = tab_data.is_crashed;
}

void HoverCardAnchorTarget::SetHoverCardDataFrom(
    const tabs::TabGroupData& group_data) {
  hover_card_data_.emplace<GroupCardData>();
  GroupCardData& card_data = std::get<GroupCardData>(hover_card_data_);

  if (group_data.num_tabs_in_group == 0) {
    return;
  }

  card_data.tab_title_data.clear();

  for (tabs::TabGroupTabData tab_data : group_data.tab_data) {
    FadeLabelViewData tab_title_label = GetTabTitleLabel(tab_data);
    tab_title_label.text =
        l10n_util::GetStringFUTF16(IDS_LIST_BULLET, tab_title_label.text);
    card_data.tab_title_data.push_back(tab_title_label);
  }

  // Now set the data for the title of the group. We need the number of
  // tabs in the group for this.
  std::u16string group_title = group_data.visual_data.title();
  if (group_title.empty()) {
    group_title = l10n_util::GetPluralStringFUTF16(
        IDS_TAB_GROUPS_UNNAMED_GROUP_HOVER_CARD_HEADER,
        group_data.num_tabs_in_group);
  } else {
    group_title = l10n_util::FormatString(
        l10n_util::GetPluralStringFUTF16(IDS_TAB_GROUPS_HOVER_CARD_HEADER,
                                         group_data.num_tabs_in_group),
        {group_title}, nullptr);
  }

  // Set the data for the number of tabs not shown in the hover card.
  card_data.group_title_data = {group_title, false};

  if (static_cast<size_t>(group_data.num_tabs_in_group) >
      tabs::TabGroupData::kMaxTabs) {
    std::u16string num_excess_str = base::NumberToString16(
        group_data.num_tabs_in_group - tabs::TabGroupData::kMaxTabs);
    card_data.excess_tab_data = {l10n_util::GetStringFUTF16(
        IDS_TAB_GROUPS_HOVER_CARD_FOOTER, num_excess_str)};
  } else {
    card_data.excess_tab_data = {u""};
  }
}

views::View* HoverCardAnchorTarget::GetAnchorView() {
  return const_cast<views::View*>(
      static_cast<const HoverCardAnchorTarget*>(this)->GetAnchorView());
}

const views::View* HoverCardAnchorTarget::GetAnchorView() const {
  return anchor_view_;
}
