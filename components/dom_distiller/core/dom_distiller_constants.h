// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_CONSTANTS_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_CONSTANTS_H_

namespace dom_distiller {

// The distillation technique used for distilling web page content.
enum class DistillerType {
  kReadability = 0,
  kDOMDistiller = 1,
  kMaxValue = kDOMDistiller,
};

extern const char kChromeUIDomDistillerURL[];
extern const char kChromeUIDomDistillerHost[];

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_CONSTANTS_H_
