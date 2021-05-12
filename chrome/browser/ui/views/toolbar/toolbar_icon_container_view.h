// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ICON_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ICON_CONTAINER_VIEW_H_

#include <list>

#include "base/observer_list.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

// A general view container for any type of toolbar icons.
class ToolbarIconContainerView : public views::View,
                                 public views::ViewObserver {
 public:
  METADATA_HEADER(ToolbarIconContainerView);

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnHighlightChanged() = 0;
  };

  explicit ToolbarIconContainerView(bool uses_highlight);
  ToolbarIconContainerView(const ToolbarIconContainerView&) = delete;
  ToolbarIconContainerView& operator=(const ToolbarIconContainerView&) = delete;
  ~ToolbarIconContainerView() override;

  // Update all the icons it contains.
  virtual void UpdateAllIcons() = 0;

  // Adds the RHS child as well as setting its margins.
  void AddMainButton(views::Button* main_button);

  // Begins observing |button| for changes that should affect the container's
  // highlight state.
  void ObserveButton(views::Button* button);

  void AddObserver(Observer* obs);
  void RemoveObserver(const Observer* obs);

  void SetIconColor(SkColor icon_color);
  SkColor GetIconColor() const;

  bool GetHighlighted() const;

  // views::View:
  void OnThemeChanged() override;

  // views::ViewObserver:
  void OnViewFocused(views::View* observed_view) override;
  void OnViewBlurred(views::View* observed_view) override;

  bool uses_highlight() const { return uses_highlight_; }

  // Provides access to the animating layout manager for subclasses.
  views::AnimatingLayoutManager* GetAnimatingLayoutManager();
  const views::AnimatingLayoutManager* GetAnimatingLayoutManager() const;

  // Provides access to the flex layout in the animating layout manager.
  views::FlexLayout* GetTargetLayoutManager();

 protected:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  friend class ToolbarAccountIconContainerViewBrowserTest;

  // Responsible for painting a roundrect border for the owning view.
  class RoundRectBorder : public ui::LayerDelegate {
   public:
    explicit RoundRectBorder(views::View* parent);
    RoundRectBorder(const RoundRectBorder&) = delete;
    RoundRectBorder& operator=(const RoundRectBorder&) = delete;

    ui::Layer* layer() { return &layer_; }

    // ui::LayerDelegate:
    void OnPaintLayer(const ui::PaintContext& context) override;
    void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                    float new_device_scale_factor) override;

   private:
    views::View* parent_;
    ui::Layer layer_;
  };

  class WidgetRestoreObserver;

  // views::View:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void AddedToWidget() override;

  void UpdateHighlight();

  // Called by |button| when its ink drop highlighted state changes.
  void OnButtonHighlightedChanged(views::Button* button);

  // Determine whether the container shows its highlight border.
  const bool uses_highlight_;

  // Hacky; see comments in UpdateHighlight().
  bool ever_painted_highlight_ = false;

  // The main view is nominally always present and is last child in the view
  // hierarchy.
  views::Button* main_button_ = nullptr;

  // Override for the icon color. If not set, |COLOR_TOOLBAR_BUTTON_ICON| is
  // used.
  base::Optional<SkColor> icon_color_;

  // Points to the child buttons that we know are currently highlighted.
  // TODO(pbos): Consider observing buttons leaving our hierarchy and removing
  // them from this set.
  std::set<const views::Button*> highlighted_buttons_;

  RoundRectBorder border_{this};

  // Tracks when the widget is restored and resets the layout.
  std::unique_ptr<WidgetRestoreObserver> restore_observer_;

  std::list<base::CallbackListSubscription> subscriptions_;

  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ICON_CONTAINER_VIEW_H_
