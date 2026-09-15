// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_CONTROLLER_H_

#include "base/callback_list.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace tabs {

// The collapse state for the vertical tab strip.
enum class VerticalTabStripCollapseState {
  kCollapsed,
  kCollapsing,
  kExpanded,
};

// The state controller for the vertical tab strip for the browser window. It
// manages the state for the vertical tab strip including display mode, collapse
// state, uncollapsed width and expand to hover setting. It is also responsible
// for serializing the state to the session service.
class VerticalTabStripStateController {
 public:
  DECLARE_USER_DATA(VerticalTabStripStateController);

  class ScopedEnableStateLock {
   public:
    ScopedEnableStateLock() = default;
    ScopedEnableStateLock(const ScopedEnableStateLock&) = delete;
    ScopedEnableStateLock& operator=(const ScopedEnableStateLock&) = delete;
    virtual ~ScopedEnableStateLock() = default;
  };

  // Delegate that is responsible for animating the collapse/expand request, and
  // updating this class's collapse state when it is done.
  class Delegate {
   public:
    virtual void SetCollapsedStateUpdatedCallback(
        base::RepeatingCallback<void(bool)> callback) = 0;
    virtual bool IsCollapsing() = 0;
    virtual void RequestCollapse(bool collapse) = 0;
  };

  virtual ~VerticalTabStripStateController();
  VerticalTabStripStateController(const VerticalTabStripStateController&) =
      delete;
  void operator=(const VerticalTabStripStateController&) = delete;

  static const VerticalTabStripStateController* From(
      const BrowserWindowInterface* browser_window);
  static VerticalTabStripStateController* From(
      BrowserWindowInterface* browser_window);

  virtual void SetDelegate(Delegate* delegate) = 0;

  virtual bool ShouldDisplayVerticalTabs() const = 0;
  virtual void SetVerticalTabsEnabled(bool enabled) = 0;

  virtual std::unique_ptr<ScopedEnableStateLock> GetEnableStateLock() = 0;

  virtual bool IsCollapsed() const = 0;
  virtual VerticalTabStripCollapseState GetCollapseState() const = 0;

  // Request that the Delegate begin transitioning its collapse state.
  // The Delegate is then responsible for updating this class's collapse
  // state through SetCollapsed.
  virtual void RequestCollapse(bool collapse) = 0;

  virtual int GetUncollapsedWidth() const = 0;
  virtual void SetUncollapsedWidth(int width) = 0;

  virtual bool IsExpandOnHoverEnabled() const = 0;
  virtual void SetExpandOnHoverEnabled(bool enabled) = 0;

  virtual bool IsResizing() const = 0;
  virtual void SetIsResizing(bool is_resizing) = 0;

  virtual const VerticalTabStripState& GetState() const = 0;

  using ResizingChangeCallback =
      base::RepeatingCallback<void(bool is_resizing)>;
  base::CallbackListSubscription RegisterOnResizingChanged(
      ResizingChangeCallback callback);

  using CollapseChangeCallback =
      base::RepeatingCallback<void(VerticalTabStripCollapseState)>;
  virtual base::CallbackListSubscription RegisterOnCollapseChanged(
      CollapseChangeCallback callback);

  base::CallbackListSubscription RegisterOnExpandOnHoverEnabledChanged(
      base::RepeatingCallback<void(bool)> callback);

  using StateChangedCallback =
      base::RepeatingCallback<void(VerticalTabStripStateController*)>;
  base::CallbackListSubscription RegisterOnModeWillChange(
      StateChangedCallback callback);
  base::CallbackListSubscription RegisterOnModeChanged(
      StateChangedCallback callback);

  static constexpr char kCollapsedKey[] = "vertical_tab_strip_collapsed";
  static constexpr char kUncollapsedWidthKey[] =
      "vertical_tab_strip_uncollapsed_width";

 protected:
  explicit VerticalTabStripStateController(BrowserWindowInterface& browser);

  void NotifyResizingChanged(bool is_resizing);
  void NotifyCollapseChanged(VerticalTabStripCollapseState new_state);
  void NotifyExpandOnHoverEnabledChanged(bool expand_on_hover_enabled);
  void NotifyModeWillChange();
  void NotifyModeChanged();

 private:
  base::RepeatingCallbackList<void(VerticalTabStripCollapseState)>
      on_collapse_changed_callback_list_;
  base::RepeatingCallbackList<void(bool)> on_resizing_changed_callback_list_;
  base::RepeatingCallbackList<void(bool)>
      on_expand_on_hover_enabled_changed_callback_list_;
  base::RepeatingCallbackList<void(VerticalTabStripStateController*)>
      on_mode_will_change_callback_list_;
  base::RepeatingCallbackList<void(VerticalTabStripStateController*)>
      on_mode_changed_callback_list_;

  ui::ScopedUnownedUserData<VerticalTabStripStateController>
      scoped_unowned_user_data_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_CONTROLLER_H_
