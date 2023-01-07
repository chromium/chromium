// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/theme_profile_key.h"

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"

namespace {

// A wrapper around Profile* that resets to nullptr when the Profile is
// destroyed, much like a WeakPtr. This is not thread-safe.
class ProfileTracker : public ProfileObserver {
 public:
  explicit ProfileTracker(Profile* profile) : profile_(profile) {
    if (profile_)
      observation_.Observe(profile_.get());
  }
  ~ProfileTracker() override = default;

  void OnProfileWillBeDestroyed(Profile* profile) override {
    observation_.Reset();
    profile_ = nullptr;
  }

  Profile* profile() { return profile_; }

 private:
  raw_ptr<Profile> profile_;
  base::ScopedObservation<Profile, ProfileObserver> observation_{this};
};

}  // namespace

DEFINE_UI_CLASS_PROPERTY_TYPE(ProfileTracker*)

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(ProfileTracker, kThemeProfileKey, nullptr)

void SetThemeProfileForWindow(aura::Window* window, Profile* profile) {
  window->SetProperty(kThemeProfileKey,
                      std::make_unique<ProfileTracker>(profile));
}

Profile* GetThemeProfileForWindow(aura::Window* window) {
  ProfileTracker* const tracker = window->GetProperty(kThemeProfileKey);
  return tracker ? tracker->profile() : nullptr;
}
