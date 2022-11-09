// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MISSIVE_MISSIVE_CLIENT_TEST_OBSERVER_H_
#define CHROMEOS_DBUS_MISSIVE_MISSIVE_CLIENT_TEST_OBSERVER_H_

#include <tuple>

#include "base/functional/callback_forward.h"
#include "base/test/repeating_test_future.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

using RecordFilterCb =
    base::RepeatingCallback<bool(const ::reporting::Record& record)>;

// Test helper class that observe |FakeMissiveClient| events.
class MissiveClientTestObserver
    : public MissiveClient::TestInterface::Observer {
 public:
  // If |destination| is specified, the observer will capture only enqueued
  // records with the specified |destination|, otherwise, all records will be
  // captured.
  explicit MissiveClientTestObserver(
      absl::optional<::reporting::Destination> destination = absl::nullopt);

  // The observer will capture only enqueued records that satisfy the condition
  // specified by |observed_record_cb|.
  explicit MissiveClientTestObserver(RecordFilterCb record_filter_cb);

  MissiveClientTestObserver(const MissiveClientTestObserver&) = delete;
  MissiveClientTestObserver operator=(const MissiveClientTestObserver&) =
      delete;

  ~MissiveClientTestObserver() override;

  void OnRecordEnqueued(::reporting::Priority priority,
                        const ::reporting::Record& record) override;

  // Wait for next |::reporting::Record| to be enqueued, remove it, and return
  // it along with the corresponding |::reporting::Priority|. Returns
  // immediately if a record is present in the queue. Times out if a
  // record does not arrive after a period of time.
  std::tuple<::reporting::Priority, ::reporting::Record>
  GetNextEnqueuedRecord();

  // Returns true immediately if there any records in the queue. Return false
  // otherwise. Does not wait for new records to arrive. Intended to be called
  // after GetNextEnqueuedRecord().
  bool HasNewEnqueuedRecords();

 private:
  base::test::RepeatingTestFuture<::reporting::Priority, ::reporting::Record>
      enqueued_records_;

  RecordFilterCb record_filter_cb_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MISSIVE_MISSIVE_CLIENT_TEST_OBSERVER_H_
