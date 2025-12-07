// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SERIAL_MOCK_SERIAL_CHOOSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_SERIAL_MOCK_SERIAL_CHOOSER_CONTROLLER_H_

#include <string>

#include "components/permissions/chooser_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockSerialChooserController : public permissions::ChooserController {
 public:
  explicit MockSerialChooserController(std::u16string title);
  ~MockSerialChooserController() override;

  MOCK_METHOD(bool, ShouldShowHelpButton, (), (const));
  MOCK_METHOD(std::u16string, GetNoOptionsText, (), (const));
  MOCK_METHOD(std::u16string, GetOkButtonLabel, (), (const));
  MOCK_METHOD((std::pair<std::u16string, std::u16string>),
              GetThrobberLabelAndTooltip,
              (),
              (const));
  MOCK_METHOD(size_t, NumOptions, (), (const));
  MOCK_METHOD(std::u16string, GetOption, (size_t), (const));
  MOCK_METHOD(bool, IsPaired, (size_t), (const));
  MOCK_METHOD(void, RefreshOptions, ());
  MOCK_METHOD(void, Select, (const std::vector<size_t>&));
  MOCK_METHOD(void, Cancel, ());
  MOCK_METHOD(void, Close, ());
  MOCK_METHOD(void, OpenAdapterOffHelpUrl, (), (const));
  MOCK_METHOD(void, OpenBluetoothPermissionHelpUrl, (), (const));
  MOCK_METHOD(void, OpenHelpCenterUrl, (), (const));
  MOCK_METHOD(void, OpenPermissionPreferences, (), (const));
  MOCK_METHOD(bool, ShouldShowAdapterOffView, (), (const));
  MOCK_METHOD(int, GetAdapterOffMessageId, (), (const));
  MOCK_METHOD(int, GetTurnAdapterOnLinkTextMessageId, (), (const));
  MOCK_METHOD(bool, ShouldShowAdapterUnauthorizedView, (), (const));
  MOCK_METHOD(int, GetBluetoothUnauthorizedMessageId, (), (const));
  MOCK_METHOD(int, GetAuthorizeBluetoothLinkTextMessageId, (), (const));
};

#endif  // CHROME_BROWSER_UI_SERIAL_MOCK_SERIAL_CHOOSER_CONTROLLER_H_
