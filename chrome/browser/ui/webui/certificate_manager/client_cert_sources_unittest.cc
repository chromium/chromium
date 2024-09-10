// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_manager/client_cert_sources.h"

#include "base/test/test_future.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ClientCertSourcesUnitTest, ClientCertStoreLoaderSimultaneousCalls) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  mojo::Remote<certificate_manager_v2::mojom::CertificateManagerPage>
      fake_page_remote;

  std::unique_ptr<CertificateManagerPageHandler::CertSource> cert_source =
      CreatePlatformClientCertSource(&fake_page_remote, &profile);
  base::test::TestFuture<
      std::vector<certificate_manager_v2::mojom::SummaryCertInfoPtr>>
      get_certs_waiter1;
  base::test::TestFuture<
      std::vector<certificate_manager_v2::mojom::SummaryCertInfoPtr>>
      get_certs_waiter2;
  cert_source->GetCertificateInfos(get_certs_waiter1.GetCallback());
  cert_source->GetCertificateInfos(get_certs_waiter2.GetCallback());
  EXPECT_TRUE(get_certs_waiter1.Wait());
  EXPECT_TRUE(get_certs_waiter2.Wait());
}
