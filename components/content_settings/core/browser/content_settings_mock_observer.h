// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_MOCK_OBSERVER_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_MOCK_OBSERVER_H_

#include "components/content_settings/core/browser/content_settings_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content_settings {

class MockObserver : public Observer {
 public:
  MockObserver();

  MockObserver(const MockObserver&) = delete;
  MockObserver& operator=(const MockObserver&) = delete;

  ~MockObserver() override;

  MOCK_METHOD3(OnContentSettingChanged,
               void(const ContentSettingsPattern& primary_pattern,
                    const ContentSettingsPattern& secondary_pattern,
                    ContentSettingsType content_type));
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_MOCK_OBSERVER_H_
