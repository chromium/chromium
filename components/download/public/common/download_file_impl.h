// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_FILE_IMPL_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_FILE_IMPL_H_

#include "components/download/public/common/download_file.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/download/public/common/base_file.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_save_info.h"
#include "components/download/public/common/rate_estimator.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/services/quarantine/public/mojom/quarantine.mojom.h"
#include "mojo/public/cpp/system/simple_watcher.h"

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
#include "components/enterprise/obfuscation/core/download_obfuscator.h"  // nogncheck
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

namespace download {

class DownloadDestinationObserver;

class COMPONENTS_DOWNLOAD_EXPORT DownloadFileImpl : public DownloadFile {
 public:
  // Takes ownership of the object pointed to by |save_info|.
  // |net_log| will be used for logging the download file's events.
  // May be constructed on any thread.  All methods besides the constructor
  // (including destruction) must occur in the same sequence.
  //
  // Note that the DownloadFileImpl automatically reads from the passed in
  // |stream|, and sends updates and status of those reads to the
  // DownloadDestinationObserver.
  DownloadFileImpl(std::unique_ptr<DownloadSaveInfo> save_info,
                   const base::FilePath& default_downloads_directory,
                   std::unique_ptr<InputStream> stream,
                   uint32_t download_id,
                   base::WeakPtr<DownloadDestinationObserver> observer);

  DownloadFileImpl(const DownloadFileImpl&) = delete;
  DownloadFileImpl& operator=(const DownloadFileImpl&) = delete;

  ~DownloadFileImpl() override;

  // DownloadFile functions.
  void Initialize(InitializeCallback initialize_callback,
                  CancelRequestCallback cancel_request_callback,
                  const DownloadItem::ReceivedSlices& received_slices) override;
  void AddInputStream(std::unique_ptr<InputStream> stream,
                      int64_t offset) override;
  void RenameAndUniquify(const base::FilePath& full_path,
                         RenameCompletionCallback callback) override;
  void RenameAndAnnotate(
      const base::FilePath& full_path,
      const std::string& client_guid,
      const GURL& source_url,
      const GURL& referrer_url,
      const std::optional<url::Origin>& request_initiator,
      mojo::PendingRemote<quarantine::mojom::Quarantine> remote_quarantine,
      RenameCompletionCallback callback) override;
  void Detach() override;
  void Cancel() override;
  void SetPotentialFileLength(int64_t length) override;
  const base::FilePath& FullPath() const override;
  bool InProgress() const override;
  void Pause() override;
  void Resume() override;

#if BUILDFLAG(IS_ANDROID)
  void PublishDownload(RenameCompletionCallback callback) override;
#endif  // BUILDFLAG(IS_ANDROID)

  // Wrapper of a ByteStreamReader or ScopedDataPipeConsumerHandle, and the meta
  // data needed to write to a slice of the target file.
  //
  // Does not require the stream reader or the consumer handle to be ready when
  // constructor is called. They can be added later when the network response
  // is handled.
  //
  // Multiple SourceStreams can concurrently write to the same file sink.
  class COMPONENTS_DOWNLOAD_EXPORT SourceStream {
   public:
    SourceStream(int64_t offset,
                 int64_t starting_file_write_offset,
                 std::unique_ptr<InputStream> stream);

    SourceStream(const SourceStream&) = delete;
    SourceStream& operator=(const SourceStream&) = delete;

    ~SourceStream();

    void Initialize();

    // Called after successfully reading and writing a buffer from stream.
    void OnBytesConsumed(int64_t bytes_read, int64_t bytes_written);

    // Given a data block that is already written, truncate the length of this
    // object to avoid overwriting that block. Data used for validation purpose
    // will not be truncated.
    void TruncateLengthWithWrittenDataBlock(int64_t received_slice_offset,
                                            int64_t bytes_written);

    // Registers the callback that will be called when data is ready.
    void RegisterDataReadyCallback(
        const mojo::SimpleWatcher::ReadyCallback& callback);
    // Clears the callback that is registered when data is ready.
    void ClearDataReadyCallback();

    // Gets the status of the input stream when the stream completes.
    // TODO(qinmin): for data pipe, it currently doesn't support sending an
    // abort status at the end. The best way to do this is to add a separate
    // mojo interface for control messages when creating this object. See
    // http://crbug.com/748240. An alternative strategy is to let the
    // DownloadManager pass the status code to DownloadItem or
    // DownloadFile. However, a DownloadFile can have multiple SourceStreams, so
    // we have to maintain a map between data pipe and
    // DownloadItem/DownloadFile somewhere.
    DownloadInterruptReason GetCompletionStatus() const;

    // Requests that on completion, `StreamSource` invokes
    // `DownloadFileImpl::OnStreamCompleted` with `this`.
    void RequestCompletionNotification(
        base::WeakPtr<DownloadFileImpl> download_file);

    InputStream::StreamState Read(scoped_refptr<net::IOBuffer>* data,
                                  size_t* length);

    // Returning the remaining bytes to validate.
    size_t GetRemainingBytesToValidate();

    int64_t offset() const { return offset_; }
    int64_t length() const { return length_; }
    int64_t starting_file_write_offset() const {
      return starting_file_write_offset_;
    }
    int64_t bytes_read() const { return bytes_read_; }
    int64_t bytes_written() const { return bytes_written_; }
    bool is_finished() const { return finished_; }
    void set_finished(bool finish) { finished_ = finish; }
    size_t index() { return index_; }
    void set_index(size_t index) { index_ = index; }
    base::CancelableOnceClosure* read_stream_callback() {
      return &read_stream_callback_;
    }

   private:
    // Starting position of the stream, this is from the network response.
    int64_t offset_;

    // The maximum length to write to the disk. If set to 0, keep writing until
    // the stream depletes.
    int64_t length_;

    // All the data received before this offset are used for validation purpose
    // and will not be written to disk. This value should always be no less than
    // |offset_|.
    int64_t starting_file_write_offset_;

    // Number of bytes read from the stream.
    // Next read position is (|offset_| + |bytes_read_|).
    int64_t bytes_read_;

    // Number of bytes written to the disk. This does not include the bytes used
    // for validation.
    int64_t bytes_written_;

    // If all the data read from the stream has been successfully written to
    // disk.
    bool finished_;

    // The slice index in the |received_slices_| vector. A slice was created
    // once the stream started writing data to the target file.
    size_t index_;

    // The stream through which data comes.
    std::unique_ptr<InputStream> input_stream_;

    // Cancelable callback to read from the |input_stream_|.
    base::CancelableOnceClosure read_stream_callback_;
  };

  // Sets the task runner for testing purpose, must be called before
  // Initialize().
  void SetTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

 protected:
  // For test class overrides.
  // Validate the first |bytes_to_validate| bytes and write the next
  // |bytes_to_write| bytes of data from the offset to the file.
  virtual DownloadInterruptReason ValidateAndWriteDataToFile(
      int64_t offset,
      const char* data,
      size_t bytes_to_validate,
      size_t bytes_to_write);

  virtual base::TimeDelta GetRetryDelayForFailedRename(int attempt_number);

  virtual bool ShouldRetryFailedRename(DownloadInterruptReason reason);

  virtual DownloadInterruptReason HandleStreamCompletionStatus(
      SourceStream* source_stream);

 private:
  friend class DownloadFileTest;

  // Options for RenameWithRetryInternal.
  enum RenameOption {
    UNIQUIFY = 1 << 0,  // If there's already a file on disk that conflicts with
                        // |new_path|, try to create a unique file by appending
                        // a uniquifier.
    ANNOTATE_WITH_SOURCE_INFORMATION = 1 << 1
  };

  struct RenameParameters {
    RenameParameters(RenameOption option,
                     const base::FilePath& new_path,
                     RenameCompletionCallback completion_callback);
    ~RenameParameters();

    RenameOption option;
    base::FilePath new_path;
    std::string client_guid;  // See BaseFile::AnnotateWithSourceInformation()
    GURL source_url;          // See BaseFile::AnnotateWithSourceInformation()
    GURL referrer_url;        // See BaseFile::AnnotateWithSourceInformation()
    std::optional<url::Origin>
        request_initiator;  // See BaseFile::AnnotateWithSourceInformation()
    mojo::PendingRemote<quarantine::mojom::Quarantine> remote_quarantine;
    int retries_left;         // RenameWithRetryInternal() will
                              // automatically retry until this
                              // count reaches 0. Each attempt
                              // decrements this counter.
    base::TimeTicks time_of_first_failure;  // Set to empty at first, but is set
                                            // when a failure is first
                                            // encountered. Used for UMA.
    RenameCompletionCallback completion_callback;
  };

  // Rename file_ based on |parameters|.
  void RenameWithRetryInternal(std::unique_ptr<RenameParameters> parameters);

  // Called after |file_| was renamed.
  void OnRenameComplete(const base::FilePath& content_path,
                        RenameCompletionCallback callback,
                        DownloadInterruptReason reason);

  // Send an update on our progress.
  void SendUpdate();

  // Called before the data is written to disk.
  void WillWriteToDisk(size_t data_len);

  // For a given SourceStream object and the bytes available to write, determine
  // the number of bytes to validate and the number of bytes it can write to the
  // disk. For parallel downloading, if the first disk IO writes to a location
  // that is already written by another stream, the current stream should stop
  // writing. Returns true if the stream can write no more data and should be
  // finished, returns false otherwise.
  bool CalculateBytesToWrite(SourceStream* source_stream,
                             size_t bytes_available_to_write,
                             size_t* bytes_to_validate,
                             size_t* bytes_to_write);

  // Called when a new SourceStream object is added.
  void OnSourceStreamAdded(SourceStream* source_stream);

  // Called when there's some activity on the input data that needs to be
  // handled.
  void StreamActive(SourceStream* source_stream, MojoResult result);

  // Register callback and start to read data from the stream.
  void RegisterAndActivateStream(SourceStream* source_stream);

  // Called when a stream completes.
  void OnStreamCompleted(SourceStream* source_stream);

  // Notify |observer_| about the download status.
  void NotifyObserver(SourceStream* source_stream,
                      DownloadInterruptReason reason,
                      InputStream::StreamState stream_state,
                      bool should_terminate);

  // Adds a new slice to |received_slices_| and update the existing entries in
  // |source_streams_| as their lengths will change.
  // TODO(qinmin): add a test for this function.
  void AddNewSlice(int64_t offset, int64_t length);

  // Check if download is completed.
  bool IsDownloadCompleted();

  // Return the total valid bytes received in the target file.
  // If the file is a sparse file, return the total number of valid bytes.
  // Otherwise, return the current file size.
  int64_t TotalBytesReceived() const;

  // Sends an error update to the observer.
  void SendErrorUpdateIfFinished(DownloadInterruptReason reason);

  // Helper method to handle stream error
  void HandleStreamError(SourceStream* source_stream,
                         DownloadInterruptReason reason);

  // Check whether this file is potentially sparse.
  bool IsSparseFile() const;

  // Given a SourceStream object, returns its neighbor that precedes it if
  // SourceStreams are ordered by their offsets.
  SourceStream* FindPrecedingNeighbor(SourceStream* source_stream);

  // See |cancel_request_callback_|.
  void CancelRequest(int64_t offset);

  // Called when the download is completed.
  void OnDownloadCompleted();

  // Print the internal states for debugging.
  void DebugStates() const;

  // The base file instance.
  BaseFile file_;

  // DownloadSaveInfo provided during construction. Since the DownloadFileImpl
  // can be created on any thread, this holds the save_info_ until it can be
  // used to initialize file_ on the download sequence.
  std::unique_ptr<DownloadSaveInfo> save_info_;

  // The default directory for creating the download file.
  base::FilePath default_download_directory_;

  // Map of the offset and the source stream that represents the slice
  // starting from offset.
  typedef std::unordered_map<int64_t, std::unique_ptr<SourceStream>>
      SourceStreams;
  SourceStreams source_streams_;

  // Used to cancel the request on UI thread, since the ByteStreamReader can't
  // close the underlying resource writing to the pipe.
  CancelRequestCallback cancel_request_callback_;

  // Used to trigger progress updates.
  std::unique_ptr<base::RepeatingTimer> update_timer_;

  // Potential file length. A range request with an offset larger than this
  // value will fail. So the actual file length cannot be larger than this.
  int64_t potential_file_length_;

  // Statistics
  size_t bytes_seen_;
  base::TimeTicks download_start_;
  RateEstimator rate_estimator_;
  int num_active_streams_;

  // The slices received, this is being updated when new data are written.
  std::vector<DownloadItem::ReceivedSlice> received_slices_;

  // Slices to download, calculated during the initialization and are not
  // updated when new data are written.
  std::vector<DownloadItem::ReceivedSlice> slice_to_download_;

  // Used to track whether the download is paused or not. This value is ignored
  // when network service is disabled as download pause/resumption is handled
  // by DownloadRequestCore in that case.
  bool is_paused_;

  uint32_t download_id_;

  // TaskRunner to post updates to the |observer_|.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // TaskRunner this object lives on after initialization.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  std::unique_ptr<enterprise_obfuscation::DownloadObfuscator> obfuscator_;
#endif

  base::WeakPtr<DownloadDestinationObserver> observer_;
  base::WeakPtrFactory<DownloadFileImpl> weak_factory_{this};
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_FILE_IMPL_H_
