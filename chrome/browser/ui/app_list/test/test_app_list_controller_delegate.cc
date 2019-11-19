// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"

#include <utility>

#include "ui/display/types/display_constants.h"
#include "ui/gfx/image/image_skia.h"

namespace test {

TestAppListControllerDelegate::TestAppListControllerDelegate() {
}

TestAppListControllerDelegate::~TestAppListControllerDelegate() {
}

int64_t TestAppListControllerDelegate::GetAppListDisplayId() {
  return display::kInvalidDisplayId;
}

void TestAppListControllerDelegate::DismissView() {}

gfx::NativeWindow TestAppListControllerDelegate::GetAppListWindow() {
  return nullptr;
}

bool TestAppListControllerDelegate::IsAppPinned(const std::string& app_id) {
  return false;
}

void TestAppListControllerDelegate::PinApp(const std::string& app_id) {
}

void TestAppListControllerDelegate::UnpinApp(const std::string& app_id) {
}

AppListControllerDelegate::Pinnable TestAppListControllerDelegate::GetPinnable(
    const std::string& app_id) {
  return NO_PIN;
}

bool TestAppListControllerDelegate::IsAppOpen(const std::string& app_id) const {
  return false;
}

bool TestAppListControllerDelegate::CanDoShowAppInfoFlow() {
  return false;
}

void TestAppListControllerDelegate::DoShowAppInfoFlow(
    Profile* profile,
    const std::string& extension_id) {
}

void TestAppListControllerDelegate::CreateNewWindow(Profile* profile,
                                                    bool incognito) {
}

void TestAppListControllerDelegate::OpenURL(Profile* profile,
                                            const GURL& url,
                                            ui::PageTransition transition,
                                            WindowOpenDisposition deposition) {
  last_opened_url_ = url;
}

void TestAppListControllerDelegate::ActivateApp(
    Profile* profile,
    const extensions::Extension* extension,
    AppListSource source,
    int event_flags) {
}

void TestAppListControllerDelegate::LaunchApp(
    Profile* profile,
    const extensions::Extension* extension,
    AppListSource source,
    int event_flags,
    int64_t display_id) {
}

}  // namespace test
