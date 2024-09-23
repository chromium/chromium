// Copyright 2014 The Chromium Authors
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

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "components/feedback/redaction_tool/redaction_tool.h"

namespace base {
class FilePath;
}

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
  // Remove a log with the specified name.
  // Returns true iff there was a log with the specified name.
  bool RemoveLog(std::string name);

  // Fill in |feedback_data| with all the data that we have collected.
  // CompressLogs() must have already been called.
  void PrepareReport(userfeedback::ExtensionSubmit* feedback_data) const;

  void RedactDescription(redaction::RedactionTool& redactor);

  // Return true if we want to include the feedback item with a key of |key| in
  // the feedback report's system logs.
  static bool IncludeInSystemLogs(const std::string& key, bool is_google_email);

  static int GetChromeBrowserProductId();

  // Mahi feature has the dedicated product id.
  static int GetMahiProductId();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  static int GetChromeOSProductId();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Getters
  const std::optional<std::string>& mac_address() const { return mac_address_; }
  const std::string& category_tag() const { return category_tag_; }
  const std::string& page_url() const { return page_url_; }
  const std::string& description() const { return description_; }
  const std::string& user_email() const { return user_email_; }
  const std::string& image() const { return image_; }
  const SystemLogsMap* sys_info() const { return &logs_; }
  int32_t product_id() const { return product_id_; }
  std::string user_agent() const { return user_agent_; }
  std::string locale() const { return locale_; }
  std::string& autofill_metadata() { return autofill_metadata_; }
  bool include_chrome_platform() const { return include_chrome_platform_; }
  const std::optional<bool>& is_offensive_or_unsafe() {
    return is_offensive_or_unsafe_;
  }
  std::string& ai_metadata() { return ai_metadata_; }

  const AttachedFile* attachment(size_t i) const { return &attachments_[i]; }
  size_t attachments() const { return attachments_.size(); }

  // Setters
  void set_mac_address(const std::optional<std::string>& mac_address) {
    mac_address_ = mac_address;
  }
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
  void set_autofill_metadata(const std::string& autofill_metadata) {
    autofill_metadata_ = autofill_metadata;
  }
  // If true, includes whether the report is from ChromeOS or Chrome on another
  // platform.
  void set_include_chrome_platform(bool include_chrome_platform) {
    include_chrome_platform_ = include_chrome_platform;
  }
  void set_is_offensive_or_unsafe(const std::optional<bool>& value) {
    is_offensive_or_unsafe_ = value;
  }
  void set_ai_metadata(const std::string& value) { ai_metadata_ = value; }

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

  std::optional<std::string> mac_address_;
  std::string category_tag_;
  std::string page_url_;
  std::string description_;
  std::string user_email_;
  int32_t product_id_;
  std::string user_agent_;
  std::string locale_;
  std::string autofill_metadata_;
  bool include_chrome_platform_ = true;
  std::optional<bool> is_offensive_or_unsafe_;
  std::string ai_metadata_;

  std::string image_;

  // It is possible that multiple attachment add calls are running in
  // parallel, so synchronize access.
  base::Lock attachments_lock_;
  std::vector<AttachedFile> attachments_;

  SystemLogsMap logs_;
};

#endif  // COMPONENTS_FEEDBACK_FEEDBACK_COMMON_H_
