// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_CONTENT_WINDOW_CONTROLS_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_CONTENT_WINDOW_CONTROLS_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace cast_receiver {

// This class defines an API provided by the embedder to control the content
// window of a specific application.
class ContentWindowControls {
 public:
  // Observer for visiblity changes in a content window.
  class VisibilityChangeObserver : public base::CheckedObserver {
   public:
    ~VisibilityChangeObserver() override;

    // Called when the associated window is shown.
    virtual void OnWindowShown() = 0;

    // Called when the associated window is hidden.
    virtual void OnWindowHidden() = 0;
  };

  ContentWindowControls();
  virtual ~ContentWindowControls();

  // Controls the visibility of the assocaited window.
  virtual void ShowWindow() = 0;
  virtual void HideWindow() = 0;

  // Controls whether touch input should be enabled for the associated window.
  virtual void EnableTouchInput() = 0;
  virtual void DisableTouchInput() = 0;

  // Adds or removes a VisibilityChangeObserver for the associated window. After
  // its addition, it will receive callbacks for all future visibility changes.
  void AddVisibilityChangeObserver(VisibilityChangeObserver& observer);
  void RemoveVisibilityChangeObserver(VisibilityChangeObserver& observer);

  // To be called when the associated window is shown or hidden, at which point
  // all observers previously added will be informed of the change.
  void OnWindowShown();
  void OnWindowHidden();

 private:
  base::ObserverList<VisibilityChangeObserver> visibility_state_observer_list_;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_CONTENT_WINDOW_CONTROLS_H_
