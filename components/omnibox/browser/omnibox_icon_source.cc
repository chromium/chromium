// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_icon_source.h"

#include "base/no_destructor.h"
#include "base/observer_list.h"

namespace omnibox {

namespace {

// Knows no icons yet, so callers use their built-in icons.
// TODO(crbug.com/587407798): Provide icons and notify observers when they
// change.
class OmniboxIconSourceImpl : public OmniboxIconSource {
 public:
  std::optional<SkBitmap> RenderBitmap(IconResourceIds id,
                                       int size_px,
                                       SkColor tint_color) override {
    return std::nullopt;
  }
  bool IsMultiColor(IconResourceIds id) override { return false; }
  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

 private:
  base::ObserverList<Observer> observers_;
};

OmniboxIconSource* g_source_for_testing = nullptr;

}  // namespace

OmniboxIconSource& GetOmniboxIconSource() {
  if (g_source_for_testing) {
    return *g_source_for_testing;
  }
  static base::NoDestructor<OmniboxIconSourceImpl> source;
  return *source;
}

base::AutoReset<OmniboxIconSource*> SetOmniboxIconSourceForTesting(
    OmniboxIconSource* source) {
  return base::AutoReset<OmniboxIconSource*>(&g_source_for_testing, source);
}

}  // namespace omnibox
