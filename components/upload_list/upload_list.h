// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPLOAD_LIST_UPLOAD_LIST_H_
#define COMPONENTS_UPLOAD_LIST_UPLOAD_LIST_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"

class CombiningUploadList;

// An UploadList is an abstraction over a list of client-side data files that
// are uploaded to a server. The UploadList allows accessing the UploadInfo
// for these files, usually to display in a UI.
//
// The UploadList loads the information asynchronously and notifies the
// client that requested the information when it is available.
class UploadList : public base::RefCountedThreadSafe<UploadList> {
 public:
  struct UploadInfo {
    enum class State {
      NotUploaded = 0,
      Pending,
      Pending_UserRequested,
      Uploaded,
    };

    UploadInfo(const UploadInfo& upload_info);
    UploadInfo(const std::string& upload_id,
               const base::Time& upload_time,
               const std::string& local_id,
               const base::Time& capture_time,
               State state);
    // Constructor for locally stored data.
    UploadInfo(const std::string& local_id,
               const base::Time& capture_time,
               State state,
               int64_t file_size);
    UploadInfo(const std::string& upload_id, const base::Time& upload_time);
    ~UploadInfo();

    // These fields are only valid when |state| == UploadInfo::State::Uploaded.
    std::string upload_id;
    base::Time upload_time;

    // ID for locally stored data that may or may not be uploaded.
    std::string local_id;

    // The time the data was captured. This is useful if the data is stored
    // locally when captured and uploaded at a later time.
    base::Time capture_time;

    State state;

    // Identifies where the crash comes from.
    std::string source;

    // The MD5sum of the path of the crash meta file.
    std::string path_hash;

    // File size for locally stored data.
    std::optional<int64_t> file_size;
  };

  UploadList();

  UploadList(const UploadList&) = delete;
  UploadList& operator=(const UploadList&) = delete;

  // Starts loading the upload list. OnUploadListAvailable will be called when
  // loading is complete. If this is called twice, the second |callback| will
  // overwrite the previously supplied one, and the first will not be called.
  void Load(base::OnceClosure callback);

  // Clears any data associated with the upload list, where the upload time or
  // capture time falls within the given range.
  void Clear(const base::Time& begin,
             const base::Time& end,
             base::OnceClosure callback = base::OnceClosure());

  // Clears any callback specified in Load().
  void CancelLoadCallback();

  // Asynchronously requests a user triggered upload.
  void RequestSingleUploadAsync(const std::string& local_id);

  // Populates |uploads| with the |max_count| most recent uploads,
  // in reverse chronological order.
  // Must be called only after a Load() callback has been received.
  // The |UploadInfo| pointers are still owned by this |UploadList| instance.
  std::vector<const UploadInfo*> GetUploads(size_t max_count) const;

 protected:
  virtual ~UploadList();

  // Reads the upload log and stores the entries in |uploads|.
  virtual std::vector<std::unique_ptr<UploadInfo>> LoadUploadList() = 0;

  // Clears data within the given time range. See Clear.
  virtual void ClearUploadList(const base::Time& begin,
                               const base::Time& end) = 0;

  // Requests a user triggered upload for a crash report with a given id.
  virtual void RequestSingleUpload(const std::string& local_id) = 0;

 private:
  friend class base::RefCountedThreadSafe<UploadList>;
  // CombiningUploadList needs to be able to call the callback functions
  // (LoadUploadList, ClearUploadList) in its callback functions.
  friend class CombiningUploadList;

  // When LoadUploadList() finishes, the results are reported in |uploads|
  // and the |load_callback_| is run.
  void OnLoadComplete(std::vector<std::unique_ptr<UploadInfo>> uploads);

  // Called when ClearUploadList() finishes.
  void OnClearComplete();

  // Ensures that this class' thread unsafe state is only accessed from the
  // sequence that owns this UploadList.
  SEQUENCE_CHECKER(sequence_checker_);

  base::OnceClosure load_callback_;
  base::OnceClosure clear_callback_;

  std::vector<std::unique_ptr<UploadInfo>> uploads_;
};

#endif  // COMPONENTS_UPLOAD_LIST_UPLOAD_LIST_H_
