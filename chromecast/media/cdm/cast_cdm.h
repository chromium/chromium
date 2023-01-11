// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CDM_CAST_CDM_H_
#define CHROMECAST_MEDIA_CDM_CAST_CDM_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chromecast/media/cdm/cast_cdm_context.h"
#include "chromecast/media/common/media_resource_tracker.h"
#include "chromecast/public/media/cast_key_status.h"
#include "media/base/callback_registry.h"
#include "media/base/content_decryption_module.h"
#include "media/cdm/json_web_key.h"

namespace chromecast {
namespace media {
class DecryptContextImpl;

// CastCdm is an extension of ContentDecryptionModule that provides common
// functionality across CDM implementations.
// All these additional functions are synchronous so:
// - either both the CDM and the media pipeline must be running on the same
//   thread,
// - or CastCdm implementations must use some locks.
//
class CastCdm : public ::media::ContentDecryptionModule {
 public:
  explicit CastCdm(MediaResourceTracker* media_resource_tracker);

  CastCdm(const CastCdm&) = delete;
  CastCdm& operator=(const CastCdm&) = delete;

  void Initialize(
      const ::media::SessionMessageCB& session_message_cb,
      const ::media::SessionClosedCB& session_closed_cb,
      const ::media::SessionKeysChangeCB& session_keys_change_cb,
      const ::media::SessionExpirationUpdateCB& session_expiration_update_cb);

  std::unique_ptr<::media::CallbackRegistration> RegisterEventCB(
      ::media::CdmContext::EventCB event_cb);

  // Returns the decryption context needed to decrypt frames encrypted with
  // |key_id|. Returns null if |key_id| is not available.
  virtual std::unique_ptr<DecryptContextImpl> GetDecryptContext(
      const std::string& key_id,
      EncryptionScheme encryption_scheme) const = 0;

  // Notifies that key status has changed (e.g. if expiry is detected by
  // hardware decoder).
  virtual void SetKeyStatus(const std::string& key_id,
                            CastKeyStatus key_status,
                            uint32_t system_code) = 0;

  // Notifies of current decoded video resolution.
  virtual void SetVideoResolution(int width, int height) = 0;

  // ::media::ContentDecryptionModule implementation.
  ::media::CdmContext* GetCdmContext() override;

  // Cast video products always provide HDCP or equivalent content protection.
  void GetStatusForPolicy(
      ::media::HdcpVersion min_hdcp_version,
      std::unique_ptr<::media::KeyStatusCdmPromise> promise) final;

 protected:
  ~CastCdm() override;

  void OnSessionMessage(const std::string& session_id,
                        const std::vector<uint8_t>& message,
                        ::media::CdmMessageType message_type);
  void OnSessionClosed(const std::string& session_id,
                       ::media::CdmSessionClosedReason reason);
  void OnSessionKeysChange(const std::string& session_id,
                           bool newly_usable_keys,
                           ::media::CdmKeysInfo keys_info);
  void OnSessionExpirationUpdate(const std::string& session_id,
                                 base::Time new_expiry_time);

  void KeyIdAndKeyPairsToInfo(const ::media::KeyIdAndKeyPairs& keys,
                              ::media::CdmKeysInfo* key_info);

 private:
  // Allow subclasses to override to provide key sysytem specific
  // initialization.
  virtual void InitializeInternal();

  ::media::SessionMessageCB session_message_cb_;
  ::media::SessionClosedCB session_closed_cb_;
  ::media::SessionKeysChangeCB session_keys_change_cb_;
  ::media::SessionExpirationUpdateCB session_expiration_update_cb_;

  // Track the usage for hardware resource. nullptr means the implementation
  // doesn't need hardware resource.
  MediaResourceTracker* const media_resource_tracker_;
  std::unique_ptr<MediaResourceTracker::ScopedUsage> media_resource_usage_;

  std::unique_ptr<CastCdmContext> cast_cdm_context_;

  ::media::CallbackRegistry<::media::CdmContext::EventCB::RunType>
      event_callbacks_;

  base::ThreadChecker thread_checker_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CDM_CAST_CDM_H_
