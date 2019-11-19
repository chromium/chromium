// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/controls/image_view.h"

class CommandUpdater;
class OmniboxView;
class PageActionIconLoadingIndicatorView;

namespace content {
class WebContents;
}

namespace gfx {
struct VectorIcon;
}

namespace views {
class BubbleDialogDelegateView;
}

// Represents an inbuilt (as opposed to an extension) page action icon that
// shows a bubble when clicked.
class PageActionIconView : public IconLabelBubbleView {
 public:
  class Delegate {
   public:
    // Gets the color to use for the ink highlight.
    virtual SkColor GetPageActionInkDropColor() const = 0;

    // Gets the opacity to use for the ink highlight.
    virtual float GetPageActionInkDropVisibleOpacity() const;

    virtual content::WebContents* GetWebContentsForPageActionIconView() = 0;

    // Returns the border the icon should use. It depends on what kind of
    // delegate this icon has.
    virtual std::unique_ptr<views::Border> CreatePageActionIconBorder() const;

    // Delegate should override and return true when the user is editing the
    // location bar contents.
    virtual bool IsLocationBarUserInputInProgress() const;

    virtual const OmniboxView* GetOmniboxView() const;
  };

  ~PageActionIconView() override;

  // Updates the color of the icon, this must be set before the icon is drawn.
  void SetIconColor(SkColor icon_color);

  void set_icon_size(int size) { icon_size_ = size; }

  // Sets the active state of the icon. An active icon will be displayed in a
  // "call to action" color.
  void SetActive(bool active);

  // Updates the visibility of the icon based on the associated model state,
  // returns whether any change occurred.
  virtual bool Update() = 0;

  // Returns the bubble instance for the icon.
  virtual views::BubbleDialogDelegateView* GetBubble() const = 0;

  // Retrieve the text to be used for a tooltip or accessible name.
  virtual base::string16 GetTextForTooltipAndAccessibleName() const = 0;

  SkColor GetLabelColorForTesting() const;

  void ExecuteForTesting();

  PageActionIconLoadingIndicatorView* loading_indicator_for_testing() {
    return loading_indicator_;
  }

 protected:
  enum ExecuteSource {
    EXECUTE_SOURCE_MOUSE,
    EXECUTE_SOURCE_KEYBOARD,
    EXECUTE_SOURCE_GESTURE,
  };

  PageActionIconView(CommandUpdater* command_updater,
                     int command_id,
                     Delegate* delegate,
                     const gfx::FontList& = gfx::FontList());

  // Returns true if a related bubble is showing.
  bool IsBubbleShowing() const override;

  // Enables or disables the associated command.
  // Returns true if the command is enabled.
  bool SetCommandEnabled(bool enabled) const;

  // Sets the tooltip text.
  void SetTooltipText(const base::string16& tooltip);

  // Invoked prior to executing the command.
  virtual void OnExecuting(ExecuteSource execute_source) = 0;

  // Invoked after the icon is pressed.
  virtual void OnPressed(bool activated) {}

  // views::IconLabelBubbleView:
  SkColor GetTextColor() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  base::string16 GetTooltipText(const gfx::Point& p) const override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;
  void OnThemeChanged() override;
  SkColor GetInkDropBaseColor() const override;
  bool ShouldShowSeparator() const final;
  void NotifyClick(const ui::Event& event) override;
  bool IsTriggerableEvent(const ui::Event& event) override;
  bool ShouldUpdateInkDropOnClickCanceled() const override;

 protected:
  // Calls OnExecuting and runs |command_id_| with a valid |command_updater_|.
  virtual void ExecuteCommand(ExecuteSource source);

  // Gets the given vector icon.
  virtual const gfx::VectorIcon& GetVectorIcon() const = 0;

  // Provides the badge to be shown on top of the vector icon, if any.
  virtual const gfx::VectorIcon& GetVectorIconBadge() const;

  // IconLabelBubbleView:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnTouchUiChanged() override;

  // Updates the icon image after some state has changed.
  virtual void UpdateIconImage();

  // Creates and updates the loading indicator.
  // TODO(crbug.com/964127): Ideally this should be lazily initialized in
  // SetIsLoading(), but local card migration icon has a weird behavior that
  // doing so will cause the indicator being invisible. Investigate and fix.
  void InstallLoadingIndicator();

  // Set if the page action icon is in the loading state.
  void SetIsLoading(bool is_loading);

  // Returns the associated web contents from the delegate.
  content::WebContents* GetWebContents() const;

  bool active() const { return active_; }

  // Delegate accessor for subclasses.
  Delegate* delegate() const { return delegate_; }

 private:
  void UpdateBorder();

  // The size of the icon image (excluding the ink drop).
  int icon_size_ = GetLayoutConstant(LOCATION_BAR_ICON_SIZE);

  // What color to paint the icon with.
  SkColor icon_color_ = gfx::kPlaceholderColor;

  // The CommandUpdater for the Browser object that owns the location bar.
  CommandUpdater* const command_updater_;

  // Delegate for access to associated state.
  Delegate* const delegate_;

  // The command ID executed when the user clicks this icon.
  const int command_id_;

  // The active node_data. The precise definition of "active" is unique to each
  // subclass, but generally indicates that the associated feature is acting on
  // the web page.
  bool active_ = false;

  // The loading indicator, showing a throbber animation on top of the icon.
  PageActionIconLoadingIndicatorView* loading_indicator_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PageActionIconView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_VIEW_H_
