// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_FEEDBACK_COMMON_H_
#define COMPONENTS_FEEDBACK_FEEDBACK_COMMON_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"

namespace userfeedback {
class ExtensionSubmit;
}

// This is the base class for FeedbackData. It primarily knows about
// data common to all feedback reports and how to zip things.
class FeedbackCommon : public base::RefCountedThreadSafe<FeedbackCommon> {
 public:
  using SystemLogsMap = std::map<std::string, std::string>;

  struct AttachedFile {
    explicit AttachedFile(const std::string& filename, std::string data);
    ~AttachedFile();

    std::string name;
    std::string data;
  };

  FeedbackCommon();

  void AddFile(const std::string& filename, std::string data);

  void AddLog(std::string name, std::string value);
  void AddLogs(SystemLogsMap logs);

  // Fill in |feedback_data| with all the data that we have collected.
  // CompressLogs() must have already been called.
  void PrepareReport(userfeedback::ExtensionSubmit* feedback_data) const;

  // Getters
  const std::string& category_tag() const { return category_tag_; }
  const std::string& page_url() const { return page_url_; }
  const std::string& description() const { return description_; }
  const std::string& user_email() const { return user_email_; }
  const std::string& image() const { return image_; }
  const SystemLogsMap* sys_info() const { return &logs_; }
  int32_t product_id() const { return product_id_; }
  std::string user_agent() const { return user_agent_; }
  std::string locale() const { return locale_; }

  const AttachedFile* attachment(size_t i) const { return &attachments_[i]; }
  size_t attachments() const { return attachments_.size(); }

  // Setters
  void set_category_tag(const std::string& category_tag) {
    category_tag_ = category_tag;
  }
  void set_page_url(const std::string& page_url) { page_url_ = page_url; }
  void set_description(const std::string& description) {
    description_ = description;
  }
  void set_user_email(const std::string& user_email) {
    user_email_ = user_email;
  }
  void set_image(std::string image) { image_ = std::move(image); }
  void set_product_id(int32_t product_id) { product_id_ = product_id; }
  void set_user_agent(const std::string& user_agent) {
    user_agent_ = user_agent;
  }
  void set_locale(const std::string& locale) { locale_ = locale; }

 protected:
  virtual ~FeedbackCommon();

  // Compresses the |data_to_be_compressed| to an attachment file to this
  // feedback data with name |zipname|. If |zipname| is empty, the |filename|
  // will be used and appended a ".zip" extension.
  void CompressFile(const base::FilePath& filename,
                    const std::string& zipname,
                    std::string data_to_be_compressed);

  void CompressLogs();

 private:
  friend class base::RefCountedThreadSafe<FeedbackCommon>;
  friend class FeedbackCommonTest;

  void AddFilesAndLogsToReport(
      userfeedback::ExtensionSubmit* feedback_data) const;

  // Returns true if a product ID was set in the feedback report.
  bool HasProductId() const { return product_id_ != -1; }

  std::string category_tag_;
  std::string page_url_;
  std::string description_;
  std::string user_email_;
  int32_t product_id_;
  std::string user_agent_;
  std::string locale_;

  std::string image_;

  // It is possible that multiple attachment add calls are running in
  // parallel, so synchronize access.
  base::Lock attachments_lock_;
  std::vector<AttachedFile> attachments_;

  SystemLogsMap logs_;
};

#endif  // COMPONENTS_FEEDBACK_FEEDBACK_COMMON_H_
