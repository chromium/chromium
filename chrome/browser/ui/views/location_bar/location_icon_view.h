// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "components/omnibox/browser/location_bar_model.h"

namespace content {
class WebContents;
}

namespace security_state {
enum SecurityLevel;
}

// Use a LocationIconView to display an icon on the leading side of the edit
// field. It shows the user's current action (while the user is editing), or the
// page security status (after navigation has completed), or extension name (if
// the URL is a chrome-extension:// URL).
class LocationIconView : public IconLabelBubbleView {
 public:
  class Delegate {
   public:
    using IconFetchedCallback =
        base::OnceCallback<void(const gfx::Image& icon)>;

    // Gets the web contents the location icon is for.
    virtual content::WebContents* GetWebContents() = 0;

    // Determines whether the omnibox (if any) is editing or empty.
    virtual bool IsEditingOrEmpty() const = 0;

    // Called when the location icon is pressed, with the event.
    virtual void OnLocationIconPressed(const ui::MouseEvent& event) {}

    // Called when the LocationIcon is dragged.
    virtual void OnLocationIconDragged(const ui::MouseEvent& event) {}

    // Returns the color to be used for the security chip in the context of
    // |security_level|.
    virtual SkColor GetSecurityChipColor(
        security_state::SecurityLevel security_level) const = 0;

    // Shows the PageInfo Dialog. This is called so that the delegate can decide
    // how and where to show the dialog. Returns true if a dialog was shown,
    // false otherwise.
    virtual bool ShowPageInfoDialog() = 0;

    // Gets the LocationBarModel.
    const virtual LocationBarModel* GetLocationBarModel() const = 0;

    // Gets an icon for the location bar icon chip.
    virtual gfx::ImageSkia GetLocationIcon(
        IconFetchedCallback on_icon_fetched) const = 0;

    // Gets the color to use for icon ink highlights.
    virtual SkColor GetLocationIconInkDropColor() const = 0;

   protected:
    virtual ~Delegate() {}
  };

  LocationIconView(const gfx::FontList& font_list, Delegate* delagate);
  ~LocationIconView() override;

  // IconLabelBubbleView:
  gfx::Size GetMinimumSize() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  SkColor GetTextColor() const override;
  bool ShouldShowSeparator() const override;
  bool ShowBubble(const ui::Event& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool IsBubbleShowing() const override;
  SkColor GetInkDropBaseColor() const override;

  // Returns what the minimum width for the label text.
  int GetMinimumLabelTextWidth() const;

  // Updates the icon's ink drop mode, focusable behavior, text and security
  // status. |suppress_animations| indicates whether this update should suppress
  // the text change animation (e.g. when swapping tabs).
  void Update(bool suppress_animations);

  // Returns text to be placed in the view.
  // - For secure/insecure pages, returns text describing the URL's security
  // level.
  // - For extension URLs, returns the extension name.
  // - For chrome:// URLs, returns the short product name (e.g. Chrome).
  // - For file:// URLs, returns the text "File".
  base::string16 GetText() const;

  // Determines whether or not text should be shown (e.g Insecure/Secure).
  // Always returns false if the text is empty or currently being edited.
  // Returns true if any of the following is true:
  // - the current page is explicitly secure or insecure.
  // - the current page has a special scheme (chrome://, extension, file://).
  bool ShouldShowText() const;

  const views::InkDrop* get_ink_drop_for_testing();

 protected:
  // IconLabelBubbleView:
  bool IsTriggerableEvent(const ui::Event& event) override;

 private:
  // The security level when the location icon was last updated. Used to decide
  // whether to animate security level transitions.
  security_state::SecurityLevel last_update_security_level_ =
      security_state::NONE;

  // Whether the delegate's editing or empty flag was set the last time the
  // location icon was updated.
  bool was_editing_or_empty_ = false;

  // Returns what the minimum size would be if the preferred size were |size|.
  gfx::Size GetMinimumSizeForPreferredSize(gfx::Size size) const;

  Delegate* delegate_;

  // Used to scope the lifetime of asynchronous icon fetch callbacks to the
  // lifetime of the object. Weak pointers issued by this factory are
  // invalidated whenever we start a new icon fetch, so don't use this weak
  // factory for any other purposes.
  base::WeakPtrFactory<LocationIconView> icon_fetch_weak_ptr_factory_{this};

  // Determines whether or not a text change should be animated.
  bool ShouldAnimateTextVisibilityChange() const;

  // Updates visibility of the text and determines whether the transition
  // (if any) should be animated.
  // If |suppress_animations| is true, the text change will not be animated.
  void UpdateTextVisibility(bool suppress_animations);

  // Updates Icon based on the current state and theme.
  void UpdateIcon();

  // Handles the arrival of an asynchronously fetched icon.
  void OnIconFetched(const gfx::Image& image);

  DISALLOW_COPY_AND_ASSIGN(LocationIconView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_VIEW_H_
