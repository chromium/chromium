// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_DISTILLER_CORE_DISTILLER_UI_HANDLE_H_
#define COMPONENTS_WEB_DISTILLER_CORE_DISTILLER_UI_HANDLE_H_

namespace web_distiller {

// ExternalFeedbackReporter handles reporting distillation quality through an
// external source.
class DistillerUIHandle {
 public:
  DistillerUIHandle() = default;
  virtual ~DistillerUIHandle() = default;

  // Open the UI settings for web distiller.
  virtual void OpenSettings() = 0;

  DistillerUIHandle(const DistillerUIHandle&) = delete;
  DistillerUIHandle& operator=(const DistillerUIHandle&) = delete;
};

}  // namespace web_distiller

#endif  // COMPONENTS_WEB_DISTILLER_CORE_DISTILLER_UI_HANDLE_H_
