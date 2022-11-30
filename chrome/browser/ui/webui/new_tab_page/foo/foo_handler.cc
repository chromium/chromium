// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/foo/foo_handler.h"
#include "base/strings/string_number_conversions.h"

FooHandler::FooHandler(mojo::PendingReceiver<foo::mojom::FooHandler> handler)
    : handler_(this, std::move(handler)) {}

FooHandler::~FooHandler() = default;

void FooHandler::GetData(GetDataCallback callback) {
  std::vector<std::tuple<std::string, std::string, std::string>> tileData(
      {{"item1", "foo",
        "https://lh4.googleusercontent.com/proxy/"
        "kFIJNnm2DMbS3B5LXaIdm2JKI6twGWwmzQbcJCfqTfuaH_"
        "ULD50v1Z3BGPEF32xTPRvgGLx492zcy_kcatCde2wmz-9ZYFqifbJRMl2DzyE=w170-"
        "h85-p-k-no-nd-mv"},
       {"item2", "bar",
        "https://lh6.googleusercontent.com/proxy/"
        "KyyCsF6dIQ783r3Znmvdo76QY2RgzcR5t4rnA5kKjsmrlpsb_pWGndQkyuAI4mv68X_"
        "9ZX2Edd-0FP4iQZRFm8UAW3oDX8Coqk3C85UNAX3H4Eh_5wGyDB0SY6HOQjOXVQ=w170-"
        "h85-p-k-no-nd-mv"},
       {"item3", "baz",
        "https://lh6.googleusercontent.com/proxy/"
        "4IP40Q18w6aDF4oS4WRnUj0MlCCKPK-vLHqSd4r-"
        "RfS6JxgblG5WJuRYpkJkoTzLMS0qv3Sxhf9wdaKkn3vHnyy6oe7Ah5y0=w170-h85-p-k-"
        "no-nd-mv"},
       {"item4", "foo",
        "https://lh3.googleusercontent.com/proxy/"
        "d_4gDNBtm9Ddv8zqqm0MVY93_j-_e5M-bGgH-"
        "bSAfIR65FYGacJTemvNp9fDT0eiIbi3bzrf7HMMsupe2QIIfm5H7BMHY3AI5rkYUpx-lQ="
        "w170-h85-p-k-no-nd-mv"}});

  std::vector<foo::mojom::FooDataItemPtr> data;
  for (const auto& datum : tileData) {
    auto data_item = foo::mojom::FooDataItem::New();
    std::tie(data_item->label, data_item->value, data_item->imageUrl) = datum;
    data.push_back(std::move(data_item));
  }
  std::move(callback).Run(std::move(data));
}
