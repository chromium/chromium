// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_STORAGE_STORAGE_UPLOADER_INTERFACE_H_
#define COMPONENTS_REPORTING_STORAGE_STORAGE_UPLOADER_INTERFACE_H_

#include <cstdint>
#include <memory>
#include <string_view>

#include "base/functional/callback.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// Interface for Upload by StorageModule.
// Must be implemented by an object returned by |StartUpload| callback (see
// below). Every time on of the StorageQueue's starts an upload (by timer or
// immediately after Write) it uses this interface to hand available records
// over to the actual uploader. StorageQueue takes ownership of it and
// automatically discards after |Completed| returns.
class UploaderInterface {
 public:
  // Reason upload is instantiated.
  enum class UploadReason : uint32_t {
    UNKNOWN = 0,           // Dummy value, should not be attached to any upload
    MANUAL = 1,            // Upload triggered by manual Flush call
    KEY_DELIVERY = 2,      // Upload requesting encryption key delivery/update
    PERIODIC = 3,          // Upload triggered by periodic queue timer
    IMMEDIATE_FLUSH = 4,   // Upload after IMMEDIATE/SECURITY event is enqueued
    FAILURE_RETRY = 5,     // Retry after any upload failure
    INCOMPLETE_RETRY = 6,  // Retry when some time after upload the events
                           // are still not confirmed by the server
    INIT_RESUME = 7,       // Automatic upload when queue initialization found
                           // the queue is not empty (some events remained after
                           // shutdown and restart)
    MAX_REASON = 8,        // Anything beyond this is illegal
  };

  // using AsyncStartUploaderCb =
  //     base::RepeatingCallback<StatusOr<std::unique_ptr<UploaderInterface>>(
  //         bool need_encryption_key)>;
  // Asynchronous callback that instantiates uploader.
  // To start upload, call |AsyncStartUploaderCb| on a thread pool. Once
  // uploader is instantiated, |AsyncStartUploaderCb| calls its parameter
  // passing uploader instance (or error Status). Set |need_encryption_key| if
  // key is needed (initially or periodically).
  using UploaderInterfaceResultCb =
      base::OnceCallback<void(StatusOr<std::unique_ptr<UploaderInterface>>)>;
  // Callback type for asynchronous UploadInterface provider.
  using AsyncStartUploaderCb =
      base::RepeatingCallback<void(UploaderInterface::UploadReason reason,
                                   UploaderInterfaceResultCb)>;

  UploaderInterface(const UploaderInterface& other) = delete;
  const UploaderInterface& operator=(const UploaderInterface& other) = delete;
  virtual ~UploaderInterface();

  // Unserializes every record and hands ownership over for processing (e.g.
  // to add to the network message). Expects |processed_cb| to be called after
  // the record or error status has been processed, with true if next record
  // needs to be delivered and false if the Uploader should stop.
  virtual void ProcessRecord(EncryptedRecord record,
                             ScopedReservation scoped_reservation,
                             base::OnceCallback<void(bool)> processed_cb) = 0;

  // Makes a note of a gap [start, start + count). Expects |processed_cb| to
  // be called after the record or error status has been processed, with true
  // if next record needs to be delivered and false if the Uploader should
  // stop.
  virtual void ProcessGap(SequenceInformation start,
                          uint64_t count,
                          base::OnceCallback<void(bool)> processed_cb) = 0;

  // Finalizes the upload (e.g. sends the message to server and gets
  // response). Called always, regardless of whether there were errors.
  virtual void Completed(Status final_status) = 0;

  static std::string_view ReasonToString(UploadReason);

 protected:
  UploaderInterface();
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_STORAGE_STORAGE_UPLOADER_INTERFACE_H_
