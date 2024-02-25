// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_STORAGE_AREA_TEST_UTIL_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_STORAGE_AREA_TEST_UTIL_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"

// Utility functions and classes for testing StorageArea implementations.

namespace storage {
namespace test {

// Creates a callback that sets the given |success_out| to the boolean argument,
// and then calls |callback|.
base::OnceCallback<void(bool)> MakeSuccessCallback(base::OnceClosure callback,
                                                   bool* success_out);

// Does a |Put| call on the given |area| and waits until the response is
// received. Returns if the call was successful.
bool PutSync(blink::mojom::StorageArea* area,
             const std::vector<uint8_t>& key,
             const std::vector<uint8_t>& value,
             const std::optional<std::vector<uint8_t>>& old_value,
             const std::string& source);

bool GetSync(blink::mojom::StorageArea* area,
             const std::vector<uint8_t>& key,
             std::vector<uint8_t>* data_out);

bool GetAllSync(blink::mojom::StorageArea* area,
                std::vector<blink::mojom::KeyValuePtr>* data_out);

// Does a |Delete| call on the area and waits until the response is
// received. Returns if the call was successful.
bool DeleteSync(blink::mojom::StorageArea* area,
                const std::vector<uint8_t>& key,
                const std::optional<std::vector<uint8_t>>& client_old_value,
                const std::string& source);

// Does a |DeleteAll| call on the area and waits until the response is
// received. Returns if the call was successful.
bool DeleteAllSync(blink::mojom::StorageArea* area, const std::string& source);

// Creates a callback that simply sets the  |*_out| variables to the arguments.
blink::mojom::StorageArea::GetAllCallback MakeGetAllCallback(
    base::OnceClosure callback,
    std::vector<blink::mojom::KeyValuePtr>* data_out);

// Mock observer implementation for use with StorageArea.
class MockLevelDBObserver : public blink::mojom::StorageAreaObserver {
 public:
  MockLevelDBObserver();
  ~MockLevelDBObserver() override;

  MOCK_METHOD4(KeyChanged,
               void(const std::vector<uint8_t>& key,
                    const std::vector<uint8_t>& new_value,
                    const std::optional<std::vector<uint8_t>>& old_value,
                    const std::string& source));
  MOCK_METHOD2(KeyChangeFailed,
               void(const std::vector<uint8_t>& key,
                    const std::string& source));
  MOCK_METHOD3(KeyDeleted,
               void(const std::vector<uint8_t>& key,
                    const std::optional<std::vector<uint8_t>>& old_value,
                    const std::string& source));
  MOCK_METHOD2(AllDeleted, void(bool was_nonempty, const std::string& source));
  MOCK_METHOD1(ShouldSendOldValueOnMutations, void(bool value));

  mojo::PendingRemote<blink::mojom::StorageAreaObserver> Bind();

 private:
  mojo::Receiver<blink::mojom::StorageAreaObserver> receiver_{this};
};

}  // namespace test
}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_STORAGE_AREA_TEST_UTIL_H_
