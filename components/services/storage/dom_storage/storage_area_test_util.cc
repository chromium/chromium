// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/storage_area_test_util.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"

namespace storage {
namespace test {

namespace {

void SuccessCallback(base::OnceClosure callback,
                     bool* success_out,
                     bool success) {
  *success_out = success;
  std::move(callback).Run();
}

}  // namespace

base::OnceCallback<void(bool)> MakeSuccessCallback(base::OnceClosure callback,
                                                   bool* success_out) {
  return base::BindOnce(&SuccessCallback, std::move(callback), success_out);
}

bool PutSync(blink::mojom::StorageArea* area,
             const std::vector<uint8_t>& key,
             const std::vector<uint8_t>& value,
             const base::Optional<std::vector<uint8_t>>& old_value,
             const std::string& source) {
  bool success = false;
  base::RunLoop loop;
  area->Put(key, value, old_value, source,
            base::BindLambdaForTesting([&](bool success_in) {
              success = success_in;
              loop.Quit();
            }));
  loop.Run();
  return success;
}

bool GetSync(blink::mojom::StorageArea* area,
             const std::vector<uint8_t>& key,
             std::vector<uint8_t>* data_out) {
  bool success = false;
  base::RunLoop loop;
  area->Get(key, base::BindLambdaForTesting(
                     [&](bool success_in, const std::vector<uint8_t>& value) {
                       success = success_in;
                       *data_out = std::move(value);
                       loop.Quit();
                     }));
  loop.Run();
  return success;
}

bool GetAllSync(blink::mojom::StorageArea* area,
                std::vector<blink::mojom::KeyValuePtr>* data_out) {
  DCHECK(data_out);
  base::RunLoop loop;
  area->GetAll(
      /*new_observer=*/mojo::NullRemote(),
      base::BindLambdaForTesting(
          [&](std::vector<blink::mojom::KeyValuePtr> data_in) {
            *data_out = std::move(data_in);
            loop.Quit();
          }));
  loop.Run();
  return true;
}

bool DeleteSync(blink::mojom::StorageArea* area,
                const std::vector<uint8_t>& key,
                const base::Optional<std::vector<uint8_t>>& client_old_value,
                const std::string& source) {
  bool success = false;
  base::RunLoop loop;
  area->Delete(key, client_old_value, source,
               base::BindLambdaForTesting([&](bool success_in) {
                 success = success_in;
                 loop.Quit();
               }));
  loop.Run();
  return success;
}

bool DeleteAllSync(blink::mojom::StorageArea* area, const std::string& source) {
  bool success = false;
  base::RunLoop loop;
  area->DeleteAll(source, /*new_observer=*/mojo::NullRemote(),
                  base::BindLambdaForTesting([&](bool success_in) {
                    success = success_in;
                    loop.Quit();
                  }));
  loop.Run();
  return success;
}

blink::mojom::StorageArea::GetAllCallback MakeGetAllCallback(
    base::OnceClosure callback,
    std::vector<blink::mojom::KeyValuePtr>* data_out) {
  DCHECK(data_out);
  return base::BindOnce(
      [](base::OnceClosure callback,
         std::vector<blink::mojom::KeyValuePtr>* data_out,
         std::vector<blink::mojom::KeyValuePtr> data_in) {
        *data_out = std::move(data_in);
        std::move(callback).Run();
      },
      std::move(callback), data_out);
}

MockLevelDBObserver::MockLevelDBObserver() = default;

MockLevelDBObserver::~MockLevelDBObserver() = default;

mojo::PendingRemote<blink::mojom::StorageAreaObserver>
MockLevelDBObserver::Bind() {
  return receiver_.BindNewPipeAndPassRemote();
}

}  // namespace test
}  // namespace storage
