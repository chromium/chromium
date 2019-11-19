// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CDM_cast_cdm_H_
#define CHROMECAST_MEDIA_CDM_cast_cdm_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chromecast/media/base/media_resource_tracker.h"
#include "chromecast/media/cdm/cast_cdm_context.h"
#include "chromecast/public/media/cast_key_status.h"
#include "media/base/cdm_context.h"
#include "media/base/content_decryption_module.h"
#include "media/base/player_tracker.h"
#include "media/cdm/json_web_key.h"

namespace media {
class PlayerTrackerImpl;
}

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

  void Initialize(
      const ::media::SessionMessageCB& session_message_cb,
      const ::media::SessionClosedCB& session_closed_cb,
      const ::media::SessionKeysChangeCB& session_keys_change_cb,
      const ::media::SessionExpirationUpdateCB& session_expiration_update_cb);

  int RegisterPlayer(const base::Closure& new_key_cb,
                     const base::Closure& cdm_unset_cb);
  void UnregisterPlayer(int registration_id);

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
  void OnSessionClosed(const std::string& session_id);
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
  std::unique_ptr<::media::PlayerTrackerImpl> player_tracker_impl_;
  std::unique_ptr<CastCdmContext> cast_cdm_context_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(CastCdm);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CDM_cast_cdm_H_
