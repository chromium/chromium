// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_XR_CONSENT_PROMPT_LEVEL_H_
#define CHROME_BROWSER_VR_SERVICE_XR_CONSENT_PROMPT_LEVEL_H_
namespace vr {

// Consent levels are incremental, granting consent for a higher level
// automatically grants consent for all levels below it.  For that reason,
// these levels should not be used in histograms so that new levels can be
// added in between the existing levels.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.vr
enum class XrConsentPromptLevel : int {
  // No consent is needed, typically this is due to the fact that no sensitive
  // information is exposed (all sensitive information is emulated, or not
  // available, such as in the "viewer" reference space).
  kNone = 0,
  // A default level of consent is needed, with no special features that need to
  // be explicitly called out.  This is typically used for entering an immersive
  // session, where there may not be an easy way out, and we want to ensure the
  // user is aware that they are doing so.
  kDefault = 1,
  // At this level of consent, the user is warned that their physical features
  // (such as height) could be exposed to the site. This is the case when a
  // site requests a "local-floor" reference space for example, where the site
  // can detect the height of the headset from the ground.
  kVRFeatures = 2,
  // At this level of consent, the user is warned that the site may be able to
  // determine the layout of their room, due to specific geometries being given
  // to the site. All methods of exposing these geometries inherently also can
  // expose the user's height, and thus this supersedes/includes features
  // being potentially exposed.
  kVRFloorPlan = 3
};

}  // namespace vr
#endif  // CHROME_BROWSER_VR_SERVICE_XR_CONSENT_PROMPT_LEVEL_H_
