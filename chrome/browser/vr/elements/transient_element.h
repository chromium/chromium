// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_TRANSIENT_ELEMENT_H_
#define CHROME_BROWSER_VR_ELEMENTS_TRANSIENT_ELEMENT_H_

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/vr_ui_export.h"

namespace vr {

// Base class for a transient element that automatically hides itself after some
// point in time. The exacly transience behavior depends on the subclass.
class VR_UI_EXPORT TransientElement : public UiElement {
 public:
  TransientElement(const TransientElement&) = delete;
  TransientElement& operator=(const TransientElement&) = delete;

  ~TransientElement() override;

  // Sets the elements visibility to the given value. If the visibility is
  // changing to true, it stays visible for the set timeout.
  void SetVisible(bool visible) override;
  void SetVisibleImmediately(bool visible) override;

  // Resets the time this element stays visible for if the element is currently
  // visible.
  void RefreshVisible();

 protected:
  explicit TransientElement(const base::TimeDelta& timeout);
  virtual void Reset();

  base::TimeDelta timeout_;
  base::TimeTicks set_visible_time_;

 private:
  typedef UiElement super;
};

// An element that hides itself after after a set timeout.
class VR_UI_EXPORT SimpleTransientElement : public TransientElement {
 public:
  explicit SimpleTransientElement(const base::TimeDelta& timeout);

  SimpleTransientElement(const SimpleTransientElement&) = delete;
  SimpleTransientElement& operator=(const SimpleTransientElement&) = delete;

  ~SimpleTransientElement() override;

 private:
  bool OnBeginFrame(const gfx::Transform& head_pose) override;

  typedef TransientElement super;
};

// The reason why a transient element hid itself. Note that this is only used by
// ShowUntilSignalTransientElement below.
enum class TransientElementHideReason : int {
  kTimeout,
  kSignal,
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_TRANSIENT_ELEMENT_H_
