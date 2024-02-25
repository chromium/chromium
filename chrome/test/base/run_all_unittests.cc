// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_io_thread.h"
#include "base/threading/platform_thread.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/test/base/chrome_unit_test_suite.h"
#include "chrome/utility/chrome_content_utility_client.h"
#include "content/public/test/unittest_test_suite.h"
#include "mojo/core/embedder/scoped_ipc_support.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) &&               \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) ||         \
     (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID) && \
      !BUILDFLAG(IS_CHROMEOS_ASH)))
#include "chrome/test/base/scoped_channel_override.h"
#elif BUILDFLAG(IS_WIN)
#include "chrome/install_static/test/scoped_install_details.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_test_helper.h"
#endif

namespace {

class ChromeContentBrowserClientWithoutNetworkServiceInitialization
    : public ChromeContentBrowserClient {
 public:
  // content::ContentBrowserClient:
  // Skip some production Network Service code that doesn't work in unit tests.
  void OnNetworkServiceCreated(
      network::mojom::NetworkService* network_service) override {}
};

std::unique_ptr<content::UnitTestTestSuite::ContentClients>
CreateContentClients() {
  auto clients = std::make_unique<content::UnitTestTestSuite::ContentClients>();
  clients->content_client = std::make_unique<ChromeContentClient>();
  clients->content_browser_client = std::make_unique<
      ChromeContentBrowserClientWithoutNetworkServiceInitialization>();
  clients->content_utility_client =
      std::make_unique<ChromeContentUtilityClient>();
  return clients;
}

}  // namespace

int main(int argc, char** argv) {
  base::PlatformThread::SetName("MainThread");

  content::UnitTestTestSuite test_suite(
      new ChromeUnitTestSuite(argc, argv),
      base::BindRepeating(CreateContentClients));

  base::TestIOThread test_io_thread(base::TestIOThread::kAutoStart);
  mojo::core::ScopedIPCSupport ipc_support(
      test_io_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) &&               \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) ||         \
     (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID) && \
      !BUILDFLAG(IS_CHROMEOS_ASH)))
  // Tests running in Google Chrome builds on Win/Mac/Lin/Lacros should present
  // as stable channel by default.
  chrome::ScopedChannelOverride scoped_channel_override(
      chrome::ScopedChannelOverride::Channel::kStable);
#elif BUILDFLAG(IS_WIN)
  // Tests running in Chromium builds on Windows need basic InstallDetails even
  // though there are no channels.
  install_static::ScopedInstallDetails scoped_install_details;
#endif

  return base::LaunchUnitTests(argc, argv,
                               base::BindOnce(&content::UnitTestTestSuite::Run,
                                              base::Unretained(&test_suite)));
}
