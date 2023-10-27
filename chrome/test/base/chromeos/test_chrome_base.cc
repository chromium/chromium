// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_chrome_base.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/allow_check_is_test_for_testing.h"
#include "chrome/browser/chrome_browser_main.h"
#include "content/public/browser/browser_main_parts.h"
#include "headless/public/headless_shell.h"
#include "ui/gfx/switches.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/test/base/chromeos/test_ash_chrome_browser_main_extra_parts.h"
#else
#include "chrome/test/base/chromeos/test_lacros_chrome_browser_main_extra_parts.h"
#endif

namespace test {

TestChromeBase::TestChromeBase(content::ContentMainParams params)
    : params_(std::move(params)) {
  base::test::AllowCheckIsTestForTesting();
  auto created_main_parts_closure =
      base::BindOnce(&TestChromeBase::CreatedBrowserMainPartsImpl,
                     weak_ptr_factory_.GetWeakPtr());
  params_.created_main_parts_closure = std::move(created_main_parts_closure);
}

TestChromeBase::~TestChromeBase() = default;

int TestChromeBase::Start() {
  // Can only Start() once.
  DCHECK(params_.created_main_parts_closure);

  int rv = 0;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kHeadless)) {
    rv = headless::HeadlessShellMain(std::move(params_));
  } else {
    rv = content::ContentMain(std::move(params_));
  }
  return rv;
}

void TestChromeBase::CreatedBrowserMainPartsImpl(
    content::BrowserMainParts* browser_main_parts) {
  browser_main_parts_ =
      static_cast<ChromeBrowserMainParts*>(browser_main_parts);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  browser_main_parts_->AddParts(
      std::make_unique<test::TestAshChromeBrowserMainExtraParts>());
#else
  browser_main_parts_->AddParts(
      std::make_unique<test::TestLacrosChromeBrowserMainExtraParts>());
#endif
}

}  // namespace test
