// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_JOB_STATUS_UPDATER_H_
#define CHROME_SERVICE_CLOUD_PRINT_JOB_STATUS_UPDATER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/service/cloud_print/cloud_print_url_fetcher.h"
#include "chrome/service/cloud_print/print_system.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace cloud_print {

// Periodically monitors the status of a local print job and updates the
// cloud print server accordingly. When the job has been completed this
// object releases the reference to itself which should cause it to
// self-destruct.
class JobStatusUpdater : public base::RefCountedThreadSafe<JobStatusUpdater>,
                         public CloudPrintURLFetcher::Delegate {
 public:
  class Delegate {
   public:
    virtual bool OnJobCompleted(JobStatusUpdater* updater) = 0;
    virtual void OnAuthError() = 0;

   protected:
    virtual ~Delegate() {}
  };

  JobStatusUpdater(const std::string& printer_name,
                   const std::string& job_id,
                   PlatformJobId local_job_id,
                   const GURL& cloud_print_server_url,
                   PrintSystem* print_system,
                   Delegate* delegate,
                   const net::PartialNetworkTrafficAnnotationTag&
                       partial_traffic_annotation);

  // Checks the status of the local print job and sends an update.
  void UpdateStatus();
  void Stop();

  // CloudPrintURLFetcher::Delegate implementation.
  CloudPrintURLFetcher::ResponseAction HandleJSONData(
      const net::URLFetcher* source,
      const GURL& url,
      const base::Value& json_data,
      bool succeeded) override;
  CloudPrintURLFetcher::ResponseAction OnRequestAuthError() override;
  std::string GetAuthHeaderValue() override;

  base::Time start_time() const {
    return start_time_;
  }

 private:
  friend class base::RefCountedThreadSafe<JobStatusUpdater>;
  ~JobStatusUpdater() override;

  base::Time start_time_;
  const std::string printer_name_;
  const std::string job_id_;
  const PlatformJobId local_job_id_;
  PrintJobDetails last_job_details_;
  scoped_refptr<CloudPrintURLFetcher> request_;
  const GURL cloud_print_server_url_;
  scoped_refptr<PrintSystem> print_system_;
  Delegate* const delegate_;
  // A flag that is set to true in Stop() and will ensure the next scheduled
  // task will do nothing.
  bool stopped_ = false;
  // Partial network traffic annotation for network requests.
  const net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation_;

  DISALLOW_COPY_AND_ASSIGN(JobStatusUpdater);
};

}  // namespace cloud_print

#endif  // CHROME_SERVICE_CLOUD_PRINT_JOB_STATUS_UPDATER_H_
