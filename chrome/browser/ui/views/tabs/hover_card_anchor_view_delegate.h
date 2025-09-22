// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_VIEWS_TABS_HOVER_CARD_ANCHOR_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_HOVER_CARD_ANCHOR_VIEW_DELEGATE_H_

#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "ui/views/view_observer.h"

class Tab;
class TabStrip;
struct TabRendererData;
namespace views {
class View;
}

// An interface that allows the TabHoverCardController to be generic and
// delegate all type-specific logic.
// Currently the only supported type is Tab, but in the future, other types of
// views will be supported, such as TabGroupHeader.
class HoverCardAnchorViewDelegate : public views::ViewObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the hover card target view is deleted or hidden. It is the
    // responsibility of the observer to delete their reference to this
    // delegate.
    virtual void OnAnchorViewRemoved() = 0;
  };

  HoverCardAnchorViewDelegate(Observer* observer, views::View* view);
  ~HoverCardAnchorViewDelegate() override;

  // Manage listeners for the hover card target view.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Determines if the view can be used as a hover card target before creation
  // of the delegate.
  static bool IsValidTargetView(const views::View* view);

  // Get the view that is being delegated.
  views::View& view() const { return view_.get(); }

  // Determines if the delegate's view can currently be used as a hover card
  // target.
  bool HasValidTargetView(const TabStrip* tab_strip) const;

  // Returns true if the delegate is observing its view.
  bool IsObserving() const;

  // Tab specific methods.
  // Returns true if the delegate's view is a tab.
  bool HasTab() const;
  // Returns the tab if the delegate's view is a tab, or nullptr otherwise.
  Tab* GetAsTab() const;
  // Returns the tab data for the delegate's view.
  TabRendererData GetTabData() const;

 private:
  // views::ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view,
                               bool visible) override;
  void OnViewIsDeleting(views::View* observed_view) override;

  const raw_ref<views::View> view_;
  base::ScopedObservation<views::View, views::ViewObserver> view_observation_{
      this};
  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_HOVER_CARD_ANCHOR_VIEW_DELEGATE_H_
