// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/views/chrome_views_test_base.h"

#include <memory>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/color/chrome_color_mixers.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "chrome/test/views/chrome_test_widget.h"
#include "components/color/color_mixers.h"
#include "content/public/test/browser_task_environment.h"
#include "ui/color/color_provider_manager.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/test/ash_test_helper.h"
#include "ui/views/test/views_test_helper_aura.h"
#endif

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<aura::test::AuraTestHelper> MakeTestHelper() {
  return std::make_unique<ash::AshTestHelper>();
}
#endif

}  // namespace

ChromeViewsTestBase::ChromeViewsTestBase()
    : views::ViewsTestBase(std::unique_ptr<base::test::TaskEnvironment>(
          std::make_unique<content::BrowserTaskEnvironment>(
              content::BrowserTaskEnvironment::MainThreadType::UI,
              content::BrowserTaskEnvironment::TimeSource::MOCK_TIME))) {}

ChromeViewsTestBase::~ChromeViewsTestBase() = default;

void ChromeViewsTestBase::SetUp() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  views::ViewsTestHelperAura::SetAuraTestHelperFactory(&MakeTestHelper);
#endif

  views::ViewsTestBase::SetUp();

  // This is similar to calling set_test_views_delegate() with a
  // ChromeTestViewsDelegate before the superclass SetUp(); however, this allows
  // the framework to provide whatever TestViewsDelegate subclass it likes as a
  // base.
  test_views_delegate()->set_layout_provider(
      ChromeLayoutProvider::CreateLayoutProvider());

  ui::ColorProviderManager::Get().AppendColorProviderInitializer(
      base::BindRepeating(color::AddComponentsColorMixers));
  ui::ColorProviderManager::Get().AppendColorProviderInitializer(
      base::BindRepeating(AddChromeColorMixers));

  // Disable navigation in the WebUI preload manager to prevent check failures
  // within the graphics and sandbox layers. These layers are not initialized in
  // views unittests.
  WebUIContentsPreloadManager::GetInstance()->DisableNavigationForTesting();
}

void ChromeViewsTestBase::TearDown() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  views::ViewsTestHelperAura::SetAuraTestHelperFactory(nullptr);
#endif

  ui::ColorProviderManager::ResetForTesting();

  views::ViewsTestBase::TearDown();
}

std::unique_ptr<views::Widget> ChromeViewsTestBase::AllocateTestWidget() {
  return std::make_unique<ChromeTestWidget>();
}
