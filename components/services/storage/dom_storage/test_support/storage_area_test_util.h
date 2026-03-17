// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_STORAGE_AREA_TEST_UTIL_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_STORAGE_AREA_TEST_UTIL_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/token.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"
#include "url/gurl.h"

// Utility functions and classes for testing StorageArea implementations.

namespace storage {
namespace test {

// Creates a StorageAreaSource for testing. Helper to avoid verbose struct
// creation at every call site.
blink::mojom::StorageAreaSourcePtr MakeStorageAreaSource(
    GURL url = GURL("https://example.url"),
    base::Token id = base::Token(1, 2));

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
             blink::mojom::StorageAreaSourcePtr source);

std::optional<std::vector<uint8_t>> GetSync(blink::mojom::StorageArea* area,
                                            const std::vector<uint8_t>& key);

std::vector<blink::mojom::KeyValuePtr> GetAllSync(
    blink::mojom::StorageArea* area);

// Does a |Delete| call on the area and waits until the response is
// received.
void DeleteSync(blink::mojom::StorageArea* area,
                const std::vector<uint8_t>& key,
                const std::optional<std::vector<uint8_t>>& client_old_value,
                blink::mojom::StorageAreaSourcePtr source);

// Does a |DeleteAll| call on the area and waits until the response is
// received.
void DeleteAllSync(blink::mojom::StorageArea* area,
                   blink::mojom::StorageAreaSourcePtr source);

// Creates a callback that simply sets the  |*_out| variables to the arguments.
blink::mojom::StorageArea::GetAllCallback MakeGetAllCallback(
    base::OnceClosure callback,
    std::vector<blink::mojom::KeyValuePtr>* data_out);

// Mock observer implementation for use with StorageArea.
class MockStorageAreaObserver : public blink::mojom::StorageAreaObserver {
 public:
  MockStorageAreaObserver();
  ~MockStorageAreaObserver() override;

  MOCK_METHOD4(KeyChanged,
               void(const std::vector<uint8_t>& key,
                    const std::vector<uint8_t>& new_value,
                    const std::optional<std::vector<uint8_t>>& old_value,
                    blink::mojom::StorageAreaSourcePtr source));
  MOCK_METHOD2(KeyChangeFailed,
               void(const std::vector<uint8_t>& key,
                    blink::mojom::StorageAreaSourcePtr source));
  MOCK_METHOD3(KeyDeleted,
               void(const std::vector<uint8_t>& key,
                    const std::optional<std::vector<uint8_t>>& old_value,
                    blink::mojom::StorageAreaSourcePtr source));
  MOCK_METHOD2(AllDeleted,
               void(bool was_nonempty,
                    blink::mojom::StorageAreaSourcePtr source));
  MOCK_METHOD1(ShouldSendOldValueOnMutations, void(bool value));

  mojo::PendingRemote<blink::mojom::StorageAreaObserver> Bind();

 private:
  mojo::Receiver<blink::mojom::StorageAreaObserver> receiver_{this};
};

}  // namespace test
}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_STORAGE_AREA_TEST_UTIL_H_
