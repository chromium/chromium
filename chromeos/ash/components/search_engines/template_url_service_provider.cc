// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/search_engines/template_url_service_provider.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"

namespace ash {
namespace {
TemplateURLServiceProvider* g_instance = nullptr;
}  // namespace

TemplateURLServiceProvider::TemplateURLServiceProvider() {
  CHECK(!g_instance);
  g_instance = this;
}

TemplateURLServiceProvider::~TemplateURLServiceProvider() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
TemplateURLServiceProvider& TemplateURLServiceProvider::Get() {
  return CHECK_DEREF(g_instance);
}

}  // namespace ash
