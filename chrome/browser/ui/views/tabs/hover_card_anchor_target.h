// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_HOVER_CARD_ANCHOR_TARGET_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_HOVER_CARD_ANCHOR_TARGET_H_

#include <variant>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "chrome/browser/ui/views/tabs/fade_footer_view.h"
#include "chrome/browser/ui/views/tabs/fade_label_view.h"
#include "ui/views/bubble/bubble_border.h"

namespace tabs {
struct TabData;
}

class TabResourceUsage;

struct TabCardData {
  TabCardData();
  ~TabCardData();
  FadeLabelViewData title_data;
  FadeLabelViewData domain_data;
  AlertFooterRowData alert_data;

  // The |CollaborationMessagingRowData| needs the Widget from the
  // TabHoverCardBubbleView so we make it in that class instead of here.
  gfx::Image collaboration_avatar;
  bool show_collaboration_messaging = false;
  std::u16string collaboration_message;

  scoped_refptr<const TabResourceUsage> tab_resource_usage;
  scoped_refptr<ThumbnailImage> thumbnail;
  bool show_discard_status = false;
  bool is_tab_discarded = false;
  bool is_crashed = false;
};

struct GroupCardData {
  GroupCardData();
  ~GroupCardData();
  FadeLabelViewData title_data;
  std::vector<FadeLabelViewData> domain_data;
};

namespace views {
class View;
}  // namespace views

// HoverCardAnchorTarget is a base class for views that display a
// TabHoverCardBubbleView on hover. It supplies the necessary data for
// anchoring and rendering, and can be implemented by views such as Tab
// and VerticalTabView.
class HoverCardAnchorTarget {
 public:
  using CardData = std::variant<std::monostate, TabCardData, GroupCardData>;

  explicit HoverCardAnchorTarget(views::View* anchor_view);
  virtual ~HoverCardAnchorTarget();

  // Returns true if this target is active.
  virtual bool NeedsToShowThumbnail() const = 0;

  // Determines if |this| is a valid target.
  virtual bool IsValidHoverCardTarget() const = 0;

  const CardData& data() const { return hover_card_data_; }

  virtual views::View* GetAnchorView();
  virtual const views::View* GetAnchorView() const;

  virtual views::BubbleBorder::Arrow GetAnchorPosition() const = 0;

 protected:
  void SetHoverCardDataFrom(const tabs::TabData& data);

 private:
  raw_ptr<views::View> anchor_view_ = nullptr;
  CardData hover_card_data_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_HOVER_CARD_ANCHOR_TARGET_H_
