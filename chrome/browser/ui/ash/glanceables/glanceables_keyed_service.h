// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_KEYED_SERVICE_H_
#define CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_KEYED_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace ash {

// Browser context keyed service that owns implementations of interfaces from
// ash/ needed to communicate with different Google services as part of
// Glanceables project.
class GlanceablesKeyedService : public KeyedService {
 public:
  GlanceablesKeyedService();
  GlanceablesKeyedService(const GlanceablesKeyedService&) = delete;
  GlanceablesKeyedService& operator=(const GlanceablesKeyedService&) = delete;
  ~GlanceablesKeyedService() override;

  // KeyedService:
  void Shutdown() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_KEYED_SERVICE_H_
