// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/gvr_consent_helper.h"

#include "base/logging.h"

namespace vr {

namespace {
GvrConsentHelper* g_instance = nullptr;
}  // namespace

GvrConsentHelper::GvrConsentHelper() = default;

void GvrConsentHelper::SetInstance(std::unique_ptr<GvrConsentHelper> instance) {
  if (g_instance)
    delete g_instance;
  g_instance = instance.release();
}

GvrConsentHelper* GvrConsentHelper::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

GvrConsentHelper::~GvrConsentHelper() = default;

}  // namespace vr
