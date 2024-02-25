// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_FEEDBACK_UPLOADER_H_
#define COMPONENTS_FEEDBACK_FEEDBACK_UPLOADER_H_

#include <list>
#include <queue>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace network {
struct ResourceRequest;
class SimpleURLLoader;
}  // namespace network

namespace feedback {

class FeedbackReport;

// FeedbackUploader is used to add a feedback report to the queue of reports
// being uploaded. In case uploading a report fails, it is written to disk and
// tried again when it's turn comes up next in the queue.
class FeedbackUploader : public KeyedService {
 public:
  // Some embedders want to delay the creation of the SharedURLLoaderFactory
  // until it is required as the creation could be expensive. In that case,
  // they can pass a callback that will be used to initialise the instance
  // out of the object creation code path.
  using SharedURLLoaderFactoryGetter =
      base::OnceCallback<scoped_refptr<network::SharedURLLoaderFactory>()>;

  FeedbackUploader(
      bool is_off_the_record,
      const base::FilePath& state_path,
      SharedURLLoaderFactoryGetter shared_url_loader_factory_getter);
  FeedbackUploader(
      bool is_off_the_record,
      const base::FilePath& state_path,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);

  FeedbackUploader(const FeedbackUploader&) = delete;
  FeedbackUploader& operator=(const FeedbackUploader&) = delete;

  ~FeedbackUploader() override;

  static void SetMinimumRetryDelayForTesting(base::TimeDelta delay);

  // Queues a report for uploading.
  // |data|: The serialized userfeedback::ExtensionSubmit proto to send.
  // |has_email|: True iff the user included their email address in the report.
  // |product_id|: The product ID for the report.
  // virtual for testing.
  virtual void QueueReport(std::unique_ptr<std::string> data,
                           bool has_email,
                           int product_id);

  // Re-queues an existing report from disk for uploading.
  void RequeueReport(scoped_refptr<FeedbackReport> report);

  bool QueueEmpty() const { return reports_queue_.empty(); }

  const base::FilePath& feedback_reports_path() const {
    return feedback_reports_path_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const {
    return task_runner_;
  }

  base::TimeDelta retry_delay() const { return retry_delay_; }

  // Deriving classes must implement this and return the appropriate
  // WeakPtr.
  virtual base::WeakPtr<FeedbackUploader> AsWeakPtr() = 0;

 protected:
  // Virtual to give implementers a chance to do work before the report is
  // disptached. Implementers can then call
  // FeedbackUploader::StartSendingReport() when ready so that the report is
  // dispatched.
  virtual void StartDispatchingReport();

  // Invoked when a feedback report upload succeeds. It will reset the
  // |retry_delay_| to its minimum value and schedules the next report upload if
  // any.
  void OnReportUploadSuccess();

  // Invoked when |report_being_dispatched_| fails to upload. If |should_retry|
  // is true, it will double the |retry_delay_| and reenqueue
  // |report_being_dispatched_| with the new delay. All subsequent retries will
  // keep increasing the delay until a successful upload is encountered.
  void OnReportUploadFailure(bool should_retry);

  const scoped_refptr<FeedbackReport>& report_being_dispatched() const {
    return report_being_dispatched_;
  }

 private:
  friend class FeedbackUploaderTest;

  // This is a std::list so that iterators remain valid during modifications.
  using UrlLoaderList = std::list<std::unique_ptr<network::SimpleURLLoader>>;

  struct ReportsUploadTimeComparator {
    bool operator()(const scoped_refptr<FeedbackReport>& a,
                    const scoped_refptr<FeedbackReport>& b) const;
  };

  // Internal constructor. Exactly one of |url_loader_factory_getter| and
  // |url_loader_factory| can be non-null.
  FeedbackUploader(
      bool is_off_the_record,
      const base::FilePath& state_path,
      SharedURLLoaderFactoryGetter url_loader_factory_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Called from DispatchReport() to give implementers a chance to add extra
  // headers to the upload request before it's sent.
  virtual void AppendExtraHeadersToUploadRequest(
      network::ResourceRequest* resource_request);

  // Uploads the |report_being_dispatched_| to be uploaded. It must
  // call either OnReportUploadSuccess() or OnReportUploadFailure() so that
  // dispatching reports can progress.
  void DispatchReport();

  void OnDispatchComplete(UrlLoaderList::iterator it,
                          std::unique_ptr<std::string> response_body);

  // Update our timer for uploading the next report.
  void UpdateUploadTimer();

  // Callback used to initialise |url_loader_factory_| lazily.
  SharedURLLoaderFactoryGetter url_loader_factory_getter_;

  // URLLoaderFactory used for network requests. May be null initially if the
  // creation is delayed (see |url_loader_factory_getter_|).
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  const base::FilePath feedback_reports_path_;

  // Timer to upload the next report at.
  base::OneShotTimer upload_timer_;

  // See comment of |FeedbackUploaderFactory::task_runner_|.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  scoped_refptr<FeedbackReport> report_being_dispatched_;

  const GURL feedback_post_url_;

  // Priority queue of reports prioritized by the time the report is supposed
  // to be uploaded at.
  std::priority_queue<scoped_refptr<FeedbackReport>,
                      std::vector<scoped_refptr<FeedbackReport>>,
                      ReportsUploadTimeComparator>
      reports_queue_;

  base::TimeDelta retry_delay_;

  // True when a report is currently being dispatched. Only a single report
  // at-a-time should be dispatched.
  bool is_dispatching_ = false;

  // Whether the feedback is associated with off-the-record context.
  const bool is_off_the_record_ = false;

  UrlLoaderList uploads_in_progress_;
};

}  // namespace feedback

#endif  // COMPONENTS_FEEDBACK_FEEDBACK_UPLOADER_H_
