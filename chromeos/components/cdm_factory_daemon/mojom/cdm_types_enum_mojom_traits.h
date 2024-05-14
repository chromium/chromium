// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_MOJOM_CDM_TYPES_ENUM_MOJOM_TRAITS_H_
#define CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_MOJOM_CDM_TYPES_ENUM_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "chromeos/components/cdm_factory_daemon/mojom/content_decryption_module.mojom.h"
#include "media/base/cdm_promise.h"
#include "media/base/content_decryption_module.h"
#include "media/base/eme_constants.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

// We do not want to use Native enum types since that will make keeping the
// mojom file consistent between Chrome and Chrome OS more difficult since we do
// not have any of those native types in Chrome OS.

namespace mojo {

template <>
struct EnumTraits<chromeos::cdm::mojom::CdmMessageType,
                  ::media::CdmMessageType> {
  static chromeos::cdm::mojom::CdmMessageType ToMojom(
      ::media::CdmMessageType input) {
    switch (input) {
      case ::media::CdmMessageType::LICENSE_REQUEST:
        return chromeos::cdm::mojom::CdmMessageType::LICENSE_REQUEST;
      case ::media::CdmMessageType::LICENSE_RENEWAL:
        return chromeos::cdm::mojom::CdmMessageType::LICENSE_RENEWAL;
      case ::media::CdmMessageType::LICENSE_RELEASE:
        return chromeos::cdm::mojom::CdmMessageType::LICENSE_RELEASE;
      case ::media::CdmMessageType::INDIVIDUALIZATION_REQUEST:
        return chromeos::cdm::mojom::CdmMessageType::INDIVIDUALIZATION_REQUEST;
    }

    NOTREACHED_IN_MIGRATION();
    return chromeos::cdm::mojom::CdmMessageType::LICENSE_REQUEST;
  }

  // Returning false results in deserialization failure and causes the
  // message pipe receiving it to be disconnected.
  static bool FromMojom(chromeos::cdm::mojom::CdmMessageType input,
                        ::media::CdmMessageType* output) {
    switch (input) {
      case chromeos::cdm::mojom::CdmMessageType::LICENSE_REQUEST:
        *output = ::media::CdmMessageType::LICENSE_REQUEST;
        return true;
      case chromeos::cdm::mojom::CdmMessageType::LICENSE_RENEWAL:
        *output = ::media::CdmMessageType::LICENSE_RENEWAL;
        return true;
      case chromeos::cdm::mojom::CdmMessageType::LICENSE_RELEASE:
        *output = ::media::CdmMessageType::LICENSE_RELEASE;
        return true;
      case chromeos::cdm::mojom::CdmMessageType::INDIVIDUALIZATION_REQUEST:
        *output = ::media::CdmMessageType::INDIVIDUALIZATION_REQUEST;
        return true;
    }

    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

template <>
struct EnumTraits<chromeos::cdm::mojom::CdmSessionType,
                  ::media::CdmSessionType> {
  static chromeos::cdm::mojom::CdmSessionType ToMojom(
      ::media::CdmSessionType input) {
    switch (input) {
      case ::media::CdmSessionType::kTemporary:
        return chromeos::cdm::mojom::CdmSessionType::kTemporary;
      case ::media::CdmSessionType::kPersistentLicense:
        return chromeos::cdm::mojom::CdmSessionType::kPersistentLicense;
    }

    NOTREACHED_IN_MIGRATION();
    return chromeos::cdm::mojom::CdmSessionType::kTemporary;
  }

  // Returning false results in deserialization failure and causes the
  // message pipe receiving it to be disconnected.
  static bool FromMojom(chromeos::cdm::mojom::CdmSessionType input,
                        ::media::CdmSessionType* output) {
    switch (input) {
      case chromeos::cdm::mojom::CdmSessionType::kTemporary:
        *output = ::media::CdmSessionType::kTemporary;
        return true;
      case chromeos::cdm::mojom::CdmSessionType::kPersistentLicense:
        *output = ::media::CdmSessionType::kPersistentLicense;
        return true;
    }

    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

template <>
struct EnumTraits<chromeos::cdm::mojom::EmeInitDataType,
                  ::media::EmeInitDataType> {
  static chromeos::cdm::mojom::EmeInitDataType ToMojom(
      ::media::EmeInitDataType input) {
    switch (input) {
      case ::media::EmeInitDataType::WEBM:
        return chromeos::cdm::mojom::EmeInitDataType::WEBM;
      case ::media::EmeInitDataType::CENC:
        return chromeos::cdm::mojom::EmeInitDataType::CENC;
      case ::media::EmeInitDataType::KEYIDS:
        return chromeos::cdm::mojom::EmeInitDataType::KEYIDS;
      case ::media::EmeInitDataType::UNKNOWN:
        return chromeos::cdm::mojom::EmeInitDataType::UNKNOWN;
    }

    NOTREACHED_IN_MIGRATION();
    return chromeos::cdm::mojom::EmeInitDataType::UNKNOWN;
  }

  // Returning false results in deserialization failure and causes the
  // message pipe receiving it to be disconnected.
  static bool FromMojom(chromeos::cdm::mojom::EmeInitDataType input,
                        ::media::EmeInitDataType* output) {
    switch (input) {
      case chromeos::cdm::mojom::EmeInitDataType::WEBM:
        *output = ::media::EmeInitDataType::WEBM;
        return true;
      case chromeos::cdm::mojom::EmeInitDataType::CENC:
        *output = ::media::EmeInitDataType::CENC;
        return true;
      case chromeos::cdm::mojom::EmeInitDataType::KEYIDS:
        *output = ::media::EmeInitDataType::KEYIDS;
        return true;
      case chromeos::cdm::mojom::EmeInitDataType::UNKNOWN:
        *output = ::media::EmeInitDataType::UNKNOWN;
        return true;
    }

    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

template <>
struct EnumTraits<chromeos::cdm::mojom::HdcpVersion, ::media::HdcpVersion> {
  static chromeos::cdm::mojom::HdcpVersion ToMojom(::media::HdcpVersion input) {
    switch (input) {
      case ::media::HdcpVersion::kHdcpVersionNone:
        return chromeos::cdm::mojom::HdcpVersion::kHdcpVersionNone;
      case ::media::HdcpVersion::kHdcpVersion1_0:
        return chromeos::cdm::mojom::HdcpVersion::kHdcpVersion1_0;
      case ::media::HdcpVersion::kHdcpVersion1_1:
        return chromeos::cdm::mojom::HdcpVersion::kHdcpVersion1_1;
      case ::media::HdcpVersion::kHdcpVersion1_2:
        return chromeos::cdm::mojom::HdcpVersion::kHdcpVersion1_2;
      case ::media::HdcpVersion::kHdcpVersion1_3:
        return chromeos::cdm::mojom::HdcpVersion::kHdcpVersion1_3;
      case ::media::HdcpVersion::kHdcpVersion1_4:
        return chromeos::cdm::mojom::HdcpVersion::kHdcpVersion1_4;
      case ::media::HdcpVersion::kHdcpVersion2_0:
        return chromeos::cdm::mojom::HdcpVersion::kHdcpVersion2_0;
      case ::media::HdcpVersion::kHdcpVersion2_1:
        return chromeos::cdm::mojom::HdcpVersion::kHdcpVersion2_1;
      case ::media::HdcpVersion::kHdcpVersion2_2:
        return chromeos::cdm::mojom::HdcpVersion::kHdcpVersion2_2;
      case ::media::HdcpVersion::kHdcpVersion2_3:
        return chromeos::cdm::mojom::HdcpVersion::kHdcpVersion2_3;
    }

    NOTREACHED_IN_MIGRATION();
    return chromeos::cdm::mojom::HdcpVersion::kHdcpVersionNone;
  }

  // Returning false results in deserialization failure and causes the
  // message pipe receiving it to be disconnected.
  static bool FromMojom(chromeos::cdm::mojom::HdcpVersion input,
                        ::media::HdcpVersion* output) {
    switch (input) {
      case chromeos::cdm::mojom::HdcpVersion::kHdcpVersionNone:
        *output = ::media::HdcpVersion::kHdcpVersionNone;
        return true;
      case chromeos::cdm::mojom::HdcpVersion::kHdcpVersion1_0:
        *output = ::media::HdcpVersion::kHdcpVersion1_0;
        return true;
      case chromeos::cdm::mojom::HdcpVersion::kHdcpVersion1_1:
        *output = ::media::HdcpVersion::kHdcpVersion1_1;
        return true;
      case chromeos::cdm::mojom::HdcpVersion::kHdcpVersion1_2:
        *output = ::media::HdcpVersion::kHdcpVersion1_2;
        return true;
      case chromeos::cdm::mojom::HdcpVersion::kHdcpVersion1_3:
        *output = ::media::HdcpVersion::kHdcpVersion1_3;
        return true;
      case chromeos::cdm::mojom::HdcpVersion::kHdcpVersion1_4:
        *output = ::media::HdcpVersion::kHdcpVersion1_4;
        return true;
      case chromeos::cdm::mojom::HdcpVersion::kHdcpVersion2_0:
        *output = ::media::HdcpVersion::kHdcpVersion2_0;
        return true;
      case chromeos::cdm::mojom::HdcpVersion::kHdcpVersion2_1:
        *output = ::media::HdcpVersion::kHdcpVersion2_1;
        return true;
      case chromeos::cdm::mojom::HdcpVersion::kHdcpVersion2_2:
        *output = ::media::HdcpVersion::kHdcpVersion2_2;
        return true;
      case chromeos::cdm::mojom::HdcpVersion::kHdcpVersion2_3:
        *output = ::media::HdcpVersion::kHdcpVersion2_3;
        return true;
    }

    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

template <>
struct EnumTraits<chromeos::cdm::mojom::PromiseException,
                  ::media::CdmPromise::Exception> {
  static chromeos::cdm::mojom::PromiseException ToMojom(
      ::media::CdmPromise::Exception input) {
    switch (input) {
      case ::media::CdmPromise::Exception::INVALID_STATE_ERROR:
        return chromeos::cdm::mojom::PromiseException::INVALID_STATE_ERROR;
      case ::media::CdmPromise::Exception::QUOTA_EXCEEDED_ERROR:
        return chromeos::cdm::mojom::PromiseException::QUOTA_EXCEEDED_ERROR;
      case ::media::CdmPromise::Exception::TYPE_ERROR:
        return chromeos::cdm::mojom::PromiseException::TYPE_ERROR;
      case ::media::CdmPromise::Exception::NOT_SUPPORTED_ERROR:
        return chromeos::cdm::mojom::PromiseException::NOT_SUPPORTED_ERROR;
    }

    NOTREACHED_IN_MIGRATION();
    return chromeos::cdm::mojom::PromiseException::INVALID_STATE_ERROR;
  }

  // Returning false results in deserialization failure and causes the
  // message pipe receiving it to be disconnected.
  static bool FromMojom(chromeos::cdm::mojom::PromiseException input,
                        ::media::CdmPromise::Exception* output) {
    switch (input) {
      case chromeos::cdm::mojom::PromiseException::INVALID_STATE_ERROR:
        *output = ::media::CdmPromise::Exception::INVALID_STATE_ERROR;
        return true;
      case chromeos::cdm::mojom::PromiseException::QUOTA_EXCEEDED_ERROR:
        *output = ::media::CdmPromise::Exception::QUOTA_EXCEEDED_ERROR;
        return true;
      case chromeos::cdm::mojom::PromiseException::TYPE_ERROR:
        *output = ::media::CdmPromise::Exception::TYPE_ERROR;
        return true;
      case chromeos::cdm::mojom::PromiseException::NOT_SUPPORTED_ERROR:
        *output = ::media::CdmPromise::Exception::NOT_SUPPORTED_ERROR;
        return true;
    }

    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

}  // namespace mojo

#endif  // CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_MOJOM_CDM_TYPES_ENUM_MOJOM_TRAITS_H_
