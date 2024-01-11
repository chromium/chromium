// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/remote_ppd_fetcher.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/test/task_environment.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

void RecordContents(RemotePpdFetcher::FetchResultCode& out_code,
                    std::string& out_contents,
                    RemotePpdFetcher::FetchResultCode fetch_code,
                    std::string fetch_contents) {
  out_code = fetch_code;
  out_contents = std::move(fetch_contents);
}

base::RepeatingCallback<network::mojom::URLLoaderFactory*()>
GetLoaderFactoryDispenser(network::TestURLLoaderFactory* loader_factory) {
  return base::BindRepeating(
      [](network::TestURLLoaderFactory* ptr)
          -> network::mojom::URLLoaderFactory* { return ptr; },
      loader_factory);
}

}  // namespace

TEST(RemotePpdFetcherTest, FetchSuccessful) {
  base::test::TaskEnvironment task_environment;
  network::TestURLLoaderFactory loader_factory;
  loader_factory.AddResponse("https://good-url", "ppd-content");
  auto ppd_fetcher =
      RemotePpdFetcher::Create(GetLoaderFactoryDispenser(&loader_factory));

  RemotePpdFetcher::FetchResultCode code;
  std::string result;
  ppd_fetcher->Fetch(
      GURL("https://good-url"),
      base::BindOnce(&RecordContents, std::ref(code), std::ref(result)));
  task_environment.RunUntilIdle();

  EXPECT_EQ(code, RemotePpdFetcher::FetchResultCode::kSuccess);
  EXPECT_EQ(result, "ppd-content");
}

TEST(RemotePpdFetcherTest, NetworkError) {
  base::test::TaskEnvironment task_environment;
  network::TestURLLoaderFactory loader_factory;
  loader_factory.AddResponse("https://bad-url", "", net::HTTP_NOT_FOUND);
  auto ppd_fetcher =
      RemotePpdFetcher::Create(GetLoaderFactoryDispenser(&loader_factory));

  RemotePpdFetcher::FetchResultCode code;
  std::string result;
  ppd_fetcher->Fetch(
      GURL("https://bad-url"),
      base::BindOnce(&RecordContents, std::ref(code), std::ref(result)));
  task_environment.RunUntilIdle();

  EXPECT_EQ(code, RemotePpdFetcher::FetchResultCode::kNetworkError);
}

}  // namespace chromeos
