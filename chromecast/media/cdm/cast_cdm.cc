// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cdm/cast_cdm.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/media/base/decrypt_context_impl.h"
#include "chromecast/media/common/media_resource_tracker.h"
#include "media/base/cdm_key_information.h"
#include "media/base/cdm_promise.h"
#include "media/base/decryptor.h"
#include "url/gurl.h"

namespace chromecast {
namespace media {
namespace {

using KeyStatus = ::media::CdmKeyInformation::KeyStatus;

constexpr size_t kKeyStatusCount =
    static_cast<size_t>(KeyStatus::KEY_STATUS_MAX) + 1;

class CastCdmContextImpl : public CastCdmContext {
 public:
  explicit CastCdmContextImpl(CastCdm* cast_cdm) : cast_cdm_(cast_cdm) {
    DCHECK(cast_cdm_);
  }

  CastCdmContextImpl(const CastCdmContextImpl&) = delete;
  CastCdmContextImpl& operator=(const CastCdmContextImpl&) = delete;

  ~CastCdmContextImpl() override = default;

  std::unique_ptr<::media::CallbackRegistration> RegisterEventCB(
      EventCB event_cb) override {
    return cast_cdm_->RegisterEventCB(std::move(event_cb));
  }

  std::unique_ptr<DecryptContextImpl> GetDecryptContext(
      const std::string& key_id,
      EncryptionScheme encryption_scheme) override {
    return cast_cdm_->GetDecryptContext(key_id, encryption_scheme);
  }

  void SetKeyStatus(const std::string& key_id,
                    CastKeyStatus key_status,
                    uint32_t system_code) override {
    cast_cdm_->SetKeyStatus(key_id, key_status, system_code);
  }

  void SetVideoResolution(int width, int height) override {
    cast_cdm_->SetVideoResolution(width, height);
  }

 private:
  // The CastCdm object which owns |this|.
  CastCdm* const cast_cdm_;
};

// Returns the HDCP version multiplied by ten.
int HdcpVersionX10(::media::HdcpVersion hdcp_version) {
  switch (hdcp_version) {
    case ::media::HdcpVersion::kHdcpVersionNone:
      return 0;
    case ::media::HdcpVersion::kHdcpVersion1_0:
      return 10;
    case ::media::HdcpVersion::kHdcpVersion1_1:
      return 11;
    case ::media::HdcpVersion::kHdcpVersion1_2:
      return 12;
    case ::media::HdcpVersion::kHdcpVersion1_3:
      return 13;
    case ::media::HdcpVersion::kHdcpVersion1_4:
      return 14;
    case ::media::HdcpVersion::kHdcpVersion2_0:
      return 20;
    case ::media::HdcpVersion::kHdcpVersion2_1:
      return 21;
    case ::media::HdcpVersion::kHdcpVersion2_2:
      return 22;
    case ::media::HdcpVersion::kHdcpVersion2_3:
      return 23;

    default:
      NOTREACHED();
  }
}

}  // namespace

CastCdm::CastCdm(MediaResourceTracker* media_resource_tracker)
    : media_resource_tracker_(media_resource_tracker),
      cast_cdm_context_(new CastCdmContextImpl(this)) {
  thread_checker_.DetachFromThread();
}

CastCdm::~CastCdm() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void CastCdm::Initialize(
    const ::media::SessionMessageCB& session_message_cb,
    const ::media::SessionClosedCB& session_closed_cb,
    const ::media::SessionKeysChangeCB& session_keys_change_cb,
    const ::media::SessionExpirationUpdateCB& session_expiration_update_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (media_resource_tracker_) {
    media_resource_usage_ = std::make_unique<MediaResourceTracker::ScopedUsage>(
        media_resource_tracker_);
  }

  session_message_cb_ = session_message_cb;
  session_closed_cb_ = session_closed_cb;
  session_keys_change_cb_ = session_keys_change_cb;
  session_expiration_update_cb_ = session_expiration_update_cb;

  InitializeInternal();
}

std::unique_ptr<::media::CallbackRegistration> CastCdm::RegisterEventCB(
    ::media::CdmContext::EventCB event_cb) {
  return event_callbacks_.Register(std::move(event_cb));
}

::media::CdmContext* CastCdm::GetCdmContext() {
  return cast_cdm_context_.get();
}

void CastCdm::GetStatusForPolicy(
    ::media::HdcpVersion min_hdcp_version,
    std::unique_ptr<::media::KeyStatusCdmPromise> promise) {
  int min_hdcp_x10 = HdcpVersionX10(min_hdcp_version);
  // TODO(sanfin): Implement a function to get the current HDCP version in the
  // browser process.
  int cur_hdcp_x10 = 0;
  promise->resolve(cur_hdcp_x10 >= min_hdcp_x10 ? KeyStatus::USABLE
                                                : KeyStatus::OUTPUT_RESTRICTED);
}

void CastCdm::OnSessionMessage(const std::string& session_id,
                               const std::vector<uint8_t>& message,
                               ::media::CdmMessageType message_type) {
  session_message_cb_.Run(session_id, message_type, message);
}

void CastCdm::OnSessionClosed(const std::string& session_id,
                              ::media::CdmSessionClosedReason reason) {
  session_closed_cb_.Run(session_id, reason);
}

void CastCdm::OnSessionKeysChange(const std::string& session_id,
                                  bool newly_usable_keys,
                                  ::media::CdmKeysInfo keys_info) {
  logging::LogMessage log_message(__FILE__, __LINE__, logging::LOGGING_INFO);
  log_message.stream() << "keystatuseschange ";
  int status_count[kKeyStatusCount] = {0};
  for (const auto& key_info : keys_info) {
    status_count[key_info->status]++;
  }
  for (int i = 0; i != ::media::CdmKeyInformation::KEY_STATUS_MAX; ++i) {
    if (status_count[i] == 0)
      continue;
    log_message.stream() << status_count[i] << " " << static_cast<KeyStatus>(i)
                         << " ";
  }

  session_keys_change_cb_.Run(session_id, newly_usable_keys,
                              std::move(keys_info));

  if (newly_usable_keys) {
    event_callbacks_.Notify(
        ::media::CdmContext::Event::kHasAdditionalUsableKey);
  }
}

void CastCdm::OnSessionExpirationUpdate(const std::string& session_id,
                                        base::Time new_expiry_time) {
  session_expiration_update_cb_.Run(session_id, new_expiry_time);
}

void CastCdm::KeyIdAndKeyPairsToInfo(const ::media::KeyIdAndKeyPairs& keys,
                                     ::media::CdmKeysInfo* keys_info) {
  DCHECK(keys_info);
  for (const std::pair<std::string, std::string>& key : keys) {
    keys_info->push_back(std::make_unique<::media::CdmKeyInformation>(
        key.first, ::media::CdmKeyInformation::USABLE, 0));
  }
}

// A default empty implementation for subclasses that don't need to provide
// any key system specific initialization.
void CastCdm::InitializeInternal() {}

}  // namespace media
}  // namespace chromecast
