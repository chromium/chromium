// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_VIEW_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_VIEW_OBSERVER_H_

#include "base/observer_list_types.h"

class MediaDialogViewObserver : public base::CheckedObserver {
 public:
  // Called when a new media session is added to the dialog.
  virtual void OnMediaSessionShown() = 0;

  // Called when a media session is hidden from the dialog.
  virtual void OnMediaSessionHidden() = 0;

  // Called when a shown media session's metadata is updated.
  virtual void OnMediaSessionMetadataUpdated() = 0;

  // Called when a shown media session's actions are changed.
  virtual void OnMediaSessionActionsChanged() = 0;

 protected:
  ~MediaDialogViewObserver() override = default;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_VIEW_OBSERVER_H_
