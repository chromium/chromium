// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/test_support/storage_area_test_util.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"

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
             const std::optional<std::vector<uint8_t>>& old_value,
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

std::optional<std::vector<uint8_t>> GetSync(blink::mojom::StorageArea* area,
                                            const std::vector<uint8_t>& key) {
  std::vector<blink::mojom::KeyValuePtr> data = GetAllSync(area);
  for (const auto& key_value : data) {
    if (key_value->key == key) {
      return key_value->value;
    }
  }
  return std::nullopt;
}

std::vector<blink::mojom::KeyValuePtr> GetAllSync(
    blink::mojom::StorageArea* area) {
  std::vector<blink::mojom::KeyValuePtr> data_out;
  base::RunLoop loop;
  area->GetAll(
      /*new_observer=*/mojo::NullRemote(),
      base::BindLambdaForTesting(
          [&](std::vector<blink::mojom::KeyValuePtr> data_in) {
            data_out = std::move(data_in);
            loop.Quit();
          }));
  loop.Run();
  return data_out;
}

void DeleteSync(blink::mojom::StorageArea* area,
                const std::vector<uint8_t>& key,
                const std::optional<std::vector<uint8_t>>& client_old_value,
                const std::string& source) {
  base::RunLoop loop;
  area->Delete(key, client_old_value, source, loop.QuitClosure());
  loop.Run();
}

void DeleteAllSync(blink::mojom::StorageArea* area, const std::string& source) {
  base::RunLoop loop;
  area->DeleteAll(source, /*new_observer=*/mojo::NullRemote(),
                  loop.QuitClosure());
  loop.Run();
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

MockStorageAreaObserver::MockStorageAreaObserver() = default;

MockStorageAreaObserver::~MockStorageAreaObserver() = default;

mojo::PendingRemote<blink::mojom::StorageAreaObserver>
MockStorageAreaObserver::Bind() {
  return receiver_.BindNewPipeAndPassRemote();
}

}  // namespace test
}  // namespace storage
