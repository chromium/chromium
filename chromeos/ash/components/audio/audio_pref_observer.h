// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_PREF_OBSERVER_H_
#define CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_PREF_OBSERVER_H_

#include "base/component_export.h"

namespace ash {

// Interface for observing audio preference changes.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_AUDIO) AudioPrefObserver {
 public:
  // Called when audio policy prefs changed.
  virtual void OnAudioPolicyPrefChanged() = 0;

 protected:
  virtual ~AudioPrefObserver() {}
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_PREF_OBSERVER_H_
