// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EYE_DROPPER_EYE_DROPPER_VIEW_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_EYE_DROPPER_EYE_DROPPER_VIEW_MAC_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/eye_dropper.h"
#include "content/public/browser/eye_dropper_listener.h"

@class NSColorSampler;

class EyeDropperViewMac : public content::EyeDropper {
 public:
  EyeDropperViewMac(content::EyeDropperListener* listener);
  EyeDropperViewMac(const EyeDropperViewMac&) = delete;
  EyeDropperViewMac& operator=(const EyeDropperViewMac&) = delete;
  ~EyeDropperViewMac() override;

 private:
  // Receives the color selection.
  raw_ptr<content::EyeDropperListener> listener_;

  NSColorSampler* __strong color_sampler_;

  base::WeakPtrFactory<EyeDropperViewMac> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EYE_DROPPER_EYE_DROPPER_VIEW_MAC_H_
