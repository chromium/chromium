// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/android/ui_controller_android_utils.h"

#include <utility>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/base64.h"
#include "base/containers/flat_map.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill_assistant/android/jni_headers/AssistantChip_jni.h"
#include "components/autofill_assistant/android/jni_headers/AssistantColor_jni.h"
#include "components/autofill_assistant/android/jni_headers/AssistantDateTime_jni.h"
#include "components/autofill_assistant/android/jni_headers/AssistantDeviceConfig_jni.h"
#include "components/autofill_assistant/android/jni_headers/AssistantDialogButton_jni.h"
#include "components/autofill_assistant/android/jni_headers/AssistantDimension_jni.h"
#include "components/autofill_assistant/android/jni_headers/AssistantDrawable_jni.h"
#include "components/autofill_assistant/android/jni_headers/AssistantInfoPopup_jni.h"
#include "components/autofill_assistant/android/jni_headers/AssistantValue_jni.h"
#include "components/autofill_assistant/android/jni_headers/AutofillAssistantDependencyInjector_jni.h"
#include "components/autofill_assistant/android/jni_headers_public/AssistantAutofillCreditCard_jni.h"
#include "components/autofill_assistant/android/jni_headers_public/AssistantAutofillProfile_jni.h"
#include "components/autofill_assistant/browser/android/client_android.h"
#include "components/autofill_assistant/browser/android/dependencies_android.h"
#include "components/autofill_assistant/browser/generic_ui_java_generated_enums.h"
#include "components/autofill_assistant/browser/service/service.h"
#include "components/autofill_assistant/browser/service/service_request_sender.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/android/gurl_android.h"

namespace autofill_assistant {
namespace ui_controller_android_utils {
namespace {

using ::base::android::ConvertJavaStringToUTF16;
using ::base::android::ConvertJavaStringToUTF8;
using ::base::android::ConvertUTF16ToJavaString;
using ::base::android::ConvertUTF8ToJavaString;
using ::base::android::JavaRef;

constexpr char kNightModePrefix[] = "night-";

DrawableIcon MapDrawableIcon(DrawableProto::Icon icon) {
  switch (icon) {
    case DrawableProto::DRAWABLE_ICON_UNDEFINED:
      return DrawableIcon::DRAWABLE_ICON_UNDEFINED;
    case DrawableProto::PROGRESSBAR_DEFAULT_INITIAL_STEP:
      return DrawableIcon::PROGRESSBAR_DEFAULT_INITIAL_STEP;
    case DrawableProto::PROGRESSBAR_DEFAULT_DATA_COLLECTION:
      return DrawableIcon::PROGRESSBAR_DEFAULT_DATA_COLLECTION;
    case DrawableProto::PROGRESSBAR_DEFAULT_PAYMENT:
      return DrawableIcon::PROGRESSBAR_DEFAULT_PAYMENT;
    case DrawableProto::PROGRESSBAR_DEFAULT_FINAL_STEP:
      return DrawableIcon::PROGRESSBAR_DEFAULT_FINAL_STEP;
    case DrawableProto::SITTING_PERSON:
      return DrawableIcon::SITTING_PERSON;
    case DrawableProto::TICKET_STUB:
      return DrawableIcon::TICKET_STUB;
    case DrawableProto::SHOPPING_BASKET:
      return DrawableIcon::SHOPPING_BASKET;
    case DrawableProto::FAST_FOOD:
      return DrawableIcon::FAST_FOOD;
    case DrawableProto::LOCAL_DINING:
      return DrawableIcon::LOCAL_DINING;
    case DrawableProto::COGWHEEL:
      return DrawableIcon::COGWHEEL;
    case DrawableProto::KEY:
      return DrawableIcon::KEY;
    case DrawableProto::CAR:
      return DrawableIcon::CAR;
    case DrawableProto::GROCERY:
      return DrawableIcon::GROCERY;
    case DrawableProto::VISIBILITY_ON:
      return DrawableIcon::VISIBILITY_ON;
    case DrawableProto::VISIBILITY_OFF:
      return DrawableIcon::VISIBILITY_OFF;
  }
}

void MaybeSetInfo(autofill::AutofillProfile* profile,
                  autofill::ServerFieldType type,
                  const JavaRef<jstring>& value,
                  const std::string& locale) {
  if (value) {
    profile->SetInfo(type, ConvertJavaStringToUTF16(value), locale);
  }
}

void MaybeSetRawInfo(autofill::AutofillProfile* profile,
                     autofill::ServerFieldType type,
                     const JavaRef<jstring>& value) {
  if (value) {
    profile->SetRawInfo(type, ConvertJavaStringToUTF16(value));
  }
}

const std::pair<std::string, std::string> RemoveDarkQualifier(
    std::string resource_qualifier,
    std::string url) {
  if (resource_qualifier.rfind(kNightModePrefix, 0) == 0) {
    resource_qualifier =
        resource_qualifier.substr(std::string(kNightModePrefix).length());
  }
  return {resource_qualifier, url};
}

std::map<std::string, std::string> FilterConfigBasedOnDayNightSetting(
    bool is_dark_mode_enabled,
    const ConfigBasedUrlProto& url_config) {
  std::map<std::string, std::string> daynight_specific_config;
  for (auto config_entry : url_config.url()) {
    std::string resource_qualifier = config_entry.first;
    std::string url = config_entry.second;
    bool dark_mode_qualified =
        resource_qualifier.rfind(kNightModePrefix, 0) == 0;
    if (is_dark_mode_enabled == dark_mode_qualified) {
      daynight_specific_config.insert(
          RemoveDarkQualifier(resource_qualifier, url));
    }
  }
  std::map<std::string, std::string> dpi_url_config;
  if (daynight_specific_config.empty()) {
    for (auto config_entry : url_config.url()) {
      daynight_specific_config.insert(
          RemoveDarkQualifier(config_entry.first, config_entry.second));
    }
  }
  return daynight_specific_config;
}

const std::string GetBitmapImageUrlBasedOnDeviceConfig(
    JNIEnv* env,
    const JavaRef<jobject>& jcontext,
    const ConfigBasedUrlProto& url_config) {
  bool is_dark_mode_enabled =
      Java_AssistantDeviceConfig_isDarkModeEnabled(env, jcontext);
  std::map<std::string, std::string> daynight_specific_config =
      FilterConfigBasedOnDayNightSetting(is_dark_mode_enabled, url_config);

  // Return image for the device pixel density. If not available, fallback to
  // mdpi. Caller has to specify mdpi image.
  std::string device_density = ConvertJavaStringToUTF8(
      Java_AssistantDeviceConfig_getDevicePixelDensity(env, jcontext));
  auto pos_it = daynight_specific_config.find(device_density);
  if (pos_it != daynight_specific_config.end()) {
    return pos_it->second;
  }
  if (daynight_specific_config.find("mdpi") == daynight_specific_config.end()) {
    return "";
  }
  return daynight_specific_config.find("mdpi")->second;
}

const std::string GetBitmapImageUrl(JNIEnv* env,
                                    const JavaRef<jobject>& jcontext,
                                    const BitmapDrawableProto& bitmap) {
  switch (bitmap.image_url_case()) {
    case BitmapDrawableProto::ImageUrlCase::kUrl: {
      return bitmap.url();
    }
    case BitmapDrawableProto::ImageUrlCase::kConfigBasedUrl: {
      return GetBitmapImageUrlBasedOnDeviceConfig(env, jcontext,
                                                  bitmap.config_based_url());
    }
    case BitmapDrawableProto::ImageUrlCase::IMAGE_URL_NOT_SET: {
      VLOG(1) << "Image url not set in bitmap image request.";
      return "";
    }
  }
}

}  // namespace

base::android::ScopedJavaLocalRef<jobject> GetJavaColor(
    JNIEnv* env,
    const std::string& color_string) {
  if (!Java_AssistantColor_isValidColorString(
          env, base::android::ConvertUTF8ToJavaString(env, color_string))) {
    if (!color_string.empty()) {
      VLOG(1) << "Encountered invalid color string: " << color_string;
    }
    return nullptr;
  }

  return Java_AssistantColor_createFromString(
      env, base::android::ConvertUTF8ToJavaString(env, color_string));
}

base::android::ScopedJavaLocalRef<jobject> GetJavaColor(
    JNIEnv* env,
    const JavaRef<jobject>& jcontext,
    const ColorProto& proto) {
  switch (proto.color_case()) {
    case ColorProto::kResourceIdentifier:
      if (!Java_AssistantColor_isValidResourceIdentifier(
              env, jcontext,
              base::android::ConvertUTF8ToJavaString(
                  env, proto.resource_identifier()))) {
        VLOG(1) << "Encountered invalid color resource identifier: "
                << proto.resource_identifier();
        return nullptr;
      }
      return Java_AssistantColor_createFromResource(
          env, jcontext,
          base::android::ConvertUTF8ToJavaString(env,
                                                 proto.resource_identifier()));
    case ColorProto::kParseableColor:
      return GetJavaColor(env, proto.parseable_color());
    case ColorProto::COLOR_NOT_SET:
      return nullptr;
  }
}

absl::optional<int> GetPixelSize(JNIEnv* env,
                                 const JavaRef<jobject>& jcontext,
                                 const ClientDimensionProto& proto) {
  switch (proto.size_case()) {
    case ClientDimensionProto::kDp:
      return Java_AssistantDimension_getPixelSizeDp(env, jcontext, proto.dp());
    case ClientDimensionProto::kWidthFactor:
      return Java_AssistantDimension_getPixelSizeWidthFactor(
          env, jcontext, proto.width_factor());
    case ClientDimensionProto::kHeightFactor:
      return Java_AssistantDimension_getPixelSizeHeightFactor(
          env, jcontext, proto.height_factor());
    case ClientDimensionProto::kSizeInPixel:
      return proto.size_in_pixel();
    case ClientDimensionProto::SIZE_NOT_SET:
      return absl::nullopt;
  }
}

int GetPixelSizeOrDefault(JNIEnv* env,
                          const JavaRef<jobject>& jcontext,
                          const ClientDimensionProto& proto,
                          int default_value) {
  auto size = GetPixelSize(env, jcontext, proto);
  if (size) {
    return *size;
  }
  return default_value;
}

base::android::ScopedJavaLocalRef<jobject> CreateJavaDrawable(
    JNIEnv* env,
    const JavaRef<jobject>& jcontext,
    const DependenciesAndroid& dependencies,
    const DrawableProto& proto,
    const UserModel* user_model) {
  switch (proto.drawable_case()) {
    case DrawableProto::kResourceIdentifier:
      if (!Java_AssistantDrawable_isValidDrawableResource(
              env, jcontext,
              base::android::ConvertUTF8ToJavaString(
                  env, proto.resource_identifier()))) {
        VLOG(1) << "Encountered invalid drawable resource identifier: "
                << proto.resource_identifier();
        return nullptr;
      }
      return Java_AssistantDrawable_createFromResource(
          env, base::android::ConvertUTF8ToJavaString(
                   env, proto.resource_identifier()));
    case DrawableProto::kBitmap: {
      std::string url = GetBitmapImageUrl(env, jcontext, proto.bitmap());
      if (proto.bitmap().use_instrinsic_dimensions()) {
        return Java_AssistantDrawable_createFromUrlWithIntrinsicDimensions(
            env, dependencies.CreateImageFetcher(),
            base::android::ConvertUTF8ToJavaString(env, url));
      } else {
        int width_pixels = ui_controller_android_utils::GetPixelSizeOrDefault(
            env, jcontext, proto.bitmap().width(), 0);
        int height_pixels = ui_controller_android_utils::GetPixelSizeOrDefault(
            env, jcontext, proto.bitmap().height(), 0);
        return Java_AssistantDrawable_createFromUrl(
            env, dependencies.CreateImageFetcher(),
            base::android::ConvertUTF8ToJavaString(env, url), width_pixels,
            height_pixels);
      }
    }
    case DrawableProto::kShape: {
      switch (proto.shape().shape_case()) {
        case ShapeDrawableProto::kRectangle: {
          auto jbackground_color = ui_controller_android_utils::GetJavaColor(
              env, jcontext, proto.shape().background_color());
          auto jstroke_color = ui_controller_android_utils::GetJavaColor(
              env, jcontext, proto.shape().stroke_color());
          int stroke_width_pixels =
              ui_controller_android_utils::GetPixelSizeOrDefault(
                  env, jcontext, proto.shape().stroke_width(), 0);
          int corner_radius_pixels =
              ui_controller_android_utils::GetPixelSizeOrDefault(
                  env, jcontext, proto.shape().rectangle().corner_radius(), 0);
          return Java_AssistantDrawable_createRectangleShape(
              env, jbackground_color, jstroke_color, stroke_width_pixels,
              corner_radius_pixels);
        }
        case ShapeDrawableProto::SHAPE_NOT_SET:
          return nullptr;
      }
    }
    case DrawableProto::kIcon: {
      return Java_AssistantDrawable_createFromIcon(
          env, static_cast<int>(MapDrawableIcon(proto.icon())));
    }
    case DrawableProto::kImageData: {
      return Java_AssistantDrawable_createFromBase64(
          env, base::android::ToJavaByteArray(env, proto.image_data()));
    }
    case DrawableProto::kImageDataBase64: {
      std::string image_data;
      if (!base::Base64Decode(proto.image_data_base64(), &image_data)) {
        VLOG(1) << "Invalid Base64 image data.";
        return nullptr;
      }
      return Java_AssistantDrawable_createFromBase64(
          env, base::android::ToJavaByteArray(env, image_data));
    }
    case DrawableProto::kFavicon: {
      if (!user_model) {
        VLOG(1) << "User model missing while trying to create a favicon.";
        return nullptr;
      }
      int diameter_size_in_pixel =
          ui_controller_android_utils::GetPixelSizeOrDefault(
              env, jcontext, proto.favicon().diameter_size(), 0);
      GURL url = proto.favicon().has_website_url()
                     ? GURL(proto.favicon().website_url())
                     : user_model->GetCurrentURL();
      return Java_AssistantDrawable_createFromFavicon(
          env, dependencies.CreateIconBridge(),
          url::GURLAndroid::FromNativeGURL(env, url), diameter_size_in_pixel,
          proto.favicon().force_monogram());
    }
    case DrawableProto::DRAWABLE_NOT_SET:
      return nullptr;
  }
}

base::android::ScopedJavaLocalRef<jobject> ToJavaValue(
    JNIEnv* env,
    const ValueProto& proto) {
  switch (proto.kind_case()) {
    case ValueProto::kStrings: {
      std::vector<std::string> strings;
      strings.reserve(proto.strings().values_size());
      for (const auto& string : proto.strings().values()) {
        strings.push_back(string);
      }
      return Java_AssistantValue_createForStrings(
          env, base::android::ToJavaArrayOfStrings(env, strings));
    }
    case ValueProto::kBooleans: {
      auto booleans = std::make_unique<bool[]>(proto.booleans().values_size());
      for (int i = 0; i < proto.booleans().values_size(); ++i) {
        booleans[i] = proto.booleans().values()[i];
      }
      return Java_AssistantValue_createForBooleans(
          env, base::android::ToJavaBooleanArray(
                   env, booleans.get(), proto.booleans().values_size()));
    }
    case ValueProto::kInts: {
      auto ints = std::make_unique<int[]>(proto.ints().values_size());
      for (int i = 0; i < proto.ints().values_size(); ++i) {
        ints[i] = proto.ints().values()[i];
      }
      return Java_AssistantValue_createForIntegers(
          env, base::android::ToJavaIntArray(env, ints.get(),
                                             proto.ints().values_size()));
    }
    case ValueProto::kCreditCards:
    case ValueProto::kProfiles:
    case ValueProto::kLoginOptions:
    case ValueProto::kCreditCardResponse:
    case ValueProto::kServerPayload:
    case ValueProto::kUserActions:
      // Unused.
      NOTREACHED();
      return nullptr;
    case ValueProto::kDates: {
      auto jlist = Java_AssistantValue_createDateTimeList(env);
      for (const auto& value : proto.dates().values()) {
        Java_AssistantValue_addDateTimeToList(
            env, jlist,
            Java_AssistantDateTime_Constructor(
                env, static_cast<int>(value.year()), value.month(), value.day(),
                0, 0, 0));
      }
      return Java_AssistantValue_createForDateTimes(env, jlist);
    }
    case ValueProto::KIND_NOT_SET:
      return Java_AssistantValue_create(env);
  }
}

ValueProto ToNativeValue(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& jvalue) {
  ValueProto proto;
  if (!jvalue) {
    return proto;
  }
  auto jints = Java_AssistantValue_getIntegers(env, jvalue);
  if (jints) {
    auto* mutable_ints = proto.mutable_ints();
    std::vector<int> ints;
    base::android::JavaIntArrayToIntVector(env, jints, &ints);
    for (int i : ints) {
      mutable_ints->add_values(i);
    }
    return proto;
  }

  auto jbooleans = Java_AssistantValue_getBooleans(env, jvalue);
  if (jbooleans) {
    auto* mutable_booleans = proto.mutable_booleans();
    std::vector<bool> booleans;
    base::android::JavaBooleanArrayToBoolVector(env, jbooleans, &booleans);
    for (auto b : booleans) {
      mutable_booleans->add_values(b);
    }
    return proto;
  }

  auto jstrings = Java_AssistantValue_getStrings(env, jvalue);
  if (jstrings) {
    auto* mutable_strings = proto.mutable_strings();
    std::vector<std::string> strings;
    base::android::AppendJavaStringArrayToStringVector(env, jstrings, &strings);
    for (const auto& string : strings) {
      mutable_strings->add_values(string);
    }
    return proto;
  }

  auto jdatetimes = Java_AssistantValue_getDateTimes(env, jvalue);
  if (jdatetimes) {
    auto* mutable_dates = proto.mutable_dates();
    for (int i = 0; i < Java_AssistantValue_getListSize(env, jdatetimes); ++i) {
      auto jdatetimes_value = Java_AssistantValue_getListAt(env, jdatetimes, i);
      DateProto date;
      date.set_year(Java_AssistantDateTime_getYear(env, jdatetimes_value));
      date.set_month(Java_AssistantDateTime_getMonth(env, jdatetimes_value));
      date.set_day(Java_AssistantDateTime_getDay(env, jdatetimes_value));
      *mutable_dates->add_values() = date;
    }
    return proto;
  }

  return proto;
}

base::android::ScopedJavaLocalRef<jobject> CreateJavaDialogButton(
    JNIEnv* env,
    const InfoPopupProto_DialogButton& button_proto,
    const JavaRef<jobject>& jinfo_page_util) {
  base::android::ScopedJavaLocalRef<jstring> jurl = nullptr;

  switch (button_proto.click_action_case()) {
    case InfoPopupProto::DialogButton::kOpenUrlInCct:
      jurl = base::android::ConvertUTF8ToJavaString(
          env, button_proto.open_url_in_cct().url());
      break;
    case InfoPopupProto::DialogButton::kCloseDialog:
      break;
    case InfoPopupProto::DialogButton::CLICK_ACTION_NOT_SET:
      NOTREACHED();
      break;
  }
  return Java_AssistantDialogButton_Constructor(
      env, jinfo_page_util,
      base::android::ConvertUTF8ToJavaString(env, button_proto.label()), jurl);
}

base::android::ScopedJavaLocalRef<jobject> CreateJavaInfoPopup(
    JNIEnv* env,
    const InfoPopupProto& info_popup_proto,
    const JavaRef<jobject>& jinfo_page_util,
    const std::string& close_display_str) {
  base::android::ScopedJavaLocalRef<jobject> jpositive_button = nullptr;
  base::android::ScopedJavaLocalRef<jobject> jnegative_button = nullptr;
  base::android::ScopedJavaLocalRef<jobject> jneutral_button = nullptr;

  if (info_popup_proto.has_positive_button() ||
      info_popup_proto.has_negative_button() ||
      info_popup_proto.has_neutral_button()) {
    if (info_popup_proto.has_positive_button()) {
      jpositive_button = CreateJavaDialogButton(
          env, info_popup_proto.positive_button(), jinfo_page_util);
    }
    if (info_popup_proto.has_negative_button()) {
      jnegative_button = CreateJavaDialogButton(
          env, info_popup_proto.negative_button(), jinfo_page_util);
    }
    if (info_popup_proto.has_neutral_button()) {
      jneutral_button = CreateJavaDialogButton(
          env, info_popup_proto.neutral_button(), jinfo_page_util);
    }
  } else {
    // If no button is set in the proto, we add a Close button
    jpositive_button = Java_AssistantDialogButton_Constructor(
        env, jinfo_page_util,
        base::android::ConvertUTF8ToJavaString(env, close_display_str),
        nullptr);
  }

  return Java_AssistantInfoPopup_Constructor(
      env,
      base::android::ConvertUTF8ToJavaString(env, info_popup_proto.title()),
      base::android::ConvertUTF8ToJavaString(env, info_popup_proto.text()),
      jpositive_button, jnegative_button, jneutral_button);
}

void ShowJavaInfoPopup(JNIEnv* env,
                       const JavaRef<jobject>& jinfo_popup,
                       const JavaRef<jobject>& jcontext) {
  Java_AssistantInfoPopup_show(env, jinfo_popup, jcontext);
}

std::string SafeConvertJavaStringToNative(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& jstring) {
  std::string native_string;
  if (jstring) {
    native_string = base::android::ConvertJavaStringToUTF8(env, jstring);
  }
  return native_string;
}

base::android::ScopedJavaLocalRef<jstring> ConvertNativeOptionalStringToJava(
    JNIEnv* env,
    const absl::optional<std::string> optional_string) {
  if (!optional_string.has_value()) {
    return nullptr;
  }
  return base::android::ConvertUTF8ToJavaString(env, *optional_string);
}

BottomSheetState ToNativeBottomSheetState(int state) {
  switch (state) {
    case 1:
      return BottomSheetState::COLLAPSED;
    case 2:
    case 3:
      return BottomSheetState::EXPANDED;
    default:
      return BottomSheetState::UNDEFINED;
  }
}

int ToJavaBottomSheetState(BottomSheetState state) {
  switch (state) {
    case BottomSheetState::COLLAPSED:
      return 1;
    case BottomSheetState::UNDEFINED:
      // The current assumption is that AutofillAssistant always starts with the
      // bottom sheet expanded.
    case BottomSheetState::EXPANDED:
      return 2;
    default:
      return -1;
  }
}

base::android::ScopedJavaLocalRef<jobject> CreateJavaAssistantChip(
    JNIEnv* env,
    const ChipProto& chip) {
  switch (chip.type()) {
    default:  // Other chip types are not supported.
      return nullptr;

    case HIGHLIGHTED_ACTION:
    case DONE_ACTION:
      return Java_AssistantChip_createHighlightedAssistantChip(
          env, chip.icon(),
          base::android::ConvertUTF8ToJavaString(env, chip.text()),
          /* disabled = */ false, chip.sticky(), /* visible = */ true,
          chip.has_content_description()
              ? base::android::ConvertUTF8ToJavaString(
                    env, chip.content_description())
              : nullptr,
          /* optionalIdentifier = */
          base::android::ConvertUTF8ToJavaString(env, std::string()));

    case NORMAL_ACTION:
    case CANCEL_ACTION:
    case CLOSE_ACTION:
    case FEEDBACK_ACTION:
      return Java_AssistantChip_createHairlineAssistantChip(
          env, chip.icon(),
          base::android::ConvertUTF8ToJavaString(env, chip.text()),
          /* disabled = */ false, chip.sticky(), /* visible = */ true,
          chip.has_content_description()
              ? base::android::ConvertUTF8ToJavaString(
                    env, chip.content_description())
              : nullptr,
          /* optionalIdentifier= */
          base::android::ConvertUTF8ToJavaString(env, std::string()));
  }
}

base::android::ScopedJavaLocalRef<jobject> CreateJavaAssistantChipList(
    JNIEnv* env,
    const std::vector<ChipProto>& chips) {
  auto jlist = Java_AssistantChip_createChipList(env);
  for (const auto& chip : chips) {
    auto jchip = CreateJavaAssistantChip(env, chip);
    if (!jchip) {
      return nullptr;
    }
    Java_AssistantChip_addChipToList(env, jlist, jchip);
  }
  return jlist;
}

base::flat_map<std::string, std::string> CreateStringMapFromJava(
    JNIEnv* env,
    const JavaRef<jobjectArray>& names,
    const JavaRef<jobjectArray>& values) {
  std::vector<std::string> names_vector;
  base::android::AppendJavaStringArrayToStringVector(env, names, &names_vector);
  std::vector<std::string> values_vector;
  base::android::AppendJavaStringArrayToStringVector(env, values,
                                                     &values_vector);
  std::vector<std::pair<std::string, std::string>> result;
  DCHECK_EQ(names_vector.size(), values_vector.size());
  for (size_t i = 0; i < names_vector.size(); ++i) {
    result.emplace_back(names_vector[i], values_vector[i]);
  }
  return base::flat_map<std::string, std::string>(std::move(result));
}

std::unique_ptr<TriggerContext> CreateTriggerContext(
    JNIEnv* env,
    content::WebContents* web_contents,
    const JavaRef<jstring>& jexperiment_ids,
    const JavaRef<jobjectArray>& jparameter_names,
    const JavaRef<jobjectArray>& jparameter_values,
    const JavaRef<jobjectArray>& jdevice_only_parameter_names,
    const JavaRef<jobjectArray>& jdevice_only_parameter_values,
    jboolean onboarding_shown,
    jboolean is_direct_action,
    const JavaRef<jstring>& jinitial_url,
    const bool is_custom_tab) {
  auto script_parameters = std::make_unique<ScriptParameters>(
      CreateStringMapFromJava(env, jparameter_names, jparameter_values));
  script_parameters->UpdateDeviceOnlyParameters(CreateStringMapFromJava(
      env, jdevice_only_parameter_names, jdevice_only_parameter_values));
  return std::make_unique<TriggerContext>(
      std::move(script_parameters),
      TriggerContext::Options(
          SafeConvertJavaStringToNative(env, jexperiment_ids), is_custom_tab,
          onboarding_shown, is_direct_action,
          SafeConvertJavaStringToNative(env, jinitial_url),
          /* is_in_chrome_triggered = */ false,
          /* is_externally_triggered = */ false,
          /* skip_autofill_assistant_onboarding = */ false,
          /* suppress_browsing_features = */ true));
}

std::unique_ptr<Service> GetServiceToInject(JNIEnv* env,
                                            ClientAndroid* client_android) {
  jlong jtest_service_to_inject =
      Java_AutofillAssistantDependencyInjector_getServiceToInject(
          env, reinterpret_cast<intptr_t>(client_android));
  std::unique_ptr<Service> test_service = nullptr;
  if (jtest_service_to_inject) {
    test_service.reset(static_cast<Service*>(
        reinterpret_cast<void*>(jtest_service_to_inject)));
  }
  return test_service;
}

std::unique_ptr<ServiceRequestSender> GetServiceRequestSenderToInject(
    JNIEnv* env) {
  jlong jtest_service_request_sender_to_inject =
      Java_AutofillAssistantDependencyInjector_getServiceRequestSenderToInject(
          env);
  std::unique_ptr<ServiceRequestSender> test_service_request_sender;
  if (jtest_service_request_sender_to_inject) {
    test_service_request_sender.reset(static_cast<ServiceRequestSender*>(
        reinterpret_cast<void*>(jtest_service_request_sender_to_inject)));
  }
  return test_service_request_sender;
}

std::unique_ptr<AutofillAssistantTtsController> GetTtsControllerToInject(
    JNIEnv* env) {
  jlong jtest_tts_controller_to_inject =
      Java_AutofillAssistantDependencyInjector_getTtsControllerToInject(env);
  std::unique_ptr<AutofillAssistantTtsController> test_tts_controller;
  if (jtest_tts_controller_to_inject) {
    test_tts_controller.reset(static_cast<AutofillAssistantTtsController*>(
        reinterpret_cast<void*>(jtest_tts_controller_to_inject)));
  }
  return test_tts_controller;
}

base::android::ScopedJavaLocalRef<jobject> CreateAssistantAutofillProfile(
    JNIEnv* env,
    const autofill::AutofillProfile& profile,
    const std::string& locale) {
  return Java_AssistantAutofillProfile_Constructor(
      env, ConvertUTF8ToJavaString(env, profile.guid()),
      ConvertUTF8ToJavaString(env, profile.origin()),
      profile.record_type() == autofill::AutofillProfile::LOCAL_PROFILE,
      ConvertUTF16ToJavaString(
          env, profile.GetInfo(autofill::NAME_HONORIFIC_PREFIX, locale)),
      ConvertUTF16ToJavaString(env,
                               profile.GetInfo(autofill::NAME_FULL, locale)),
      ConvertUTF16ToJavaString(env, profile.GetRawInfo(autofill::COMPANY_NAME)),
      ConvertUTF16ToJavaString(
          env, profile.GetRawInfo(autofill::ADDRESS_HOME_STREET_ADDRESS)),
      ConvertUTF16ToJavaString(
          env, profile.GetRawInfo(autofill::ADDRESS_HOME_STATE)),
      ConvertUTF16ToJavaString(env,
                               profile.GetRawInfo(autofill::ADDRESS_HOME_CITY)),
      ConvertUTF16ToJavaString(
          env, profile.GetRawInfo(autofill::ADDRESS_HOME_DEPENDENT_LOCALITY)),
      ConvertUTF16ToJavaString(env,
                               profile.GetRawInfo(autofill::ADDRESS_HOME_ZIP)),
      ConvertUTF16ToJavaString(
          env, profile.GetRawInfo(autofill::ADDRESS_HOME_SORTING_CODE)),
      ConvertUTF16ToJavaString(
          env, profile.GetRawInfo(autofill::ADDRESS_HOME_COUNTRY)),
      ConvertUTF16ToJavaString(
          env, profile.GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER)),
      ConvertUTF16ToJavaString(env,
                               profile.GetRawInfo(autofill::EMAIL_ADDRESS)),
      ConvertUTF8ToJavaString(env, profile.language_code()));
}

void PopulateAutofillProfileFromJava(
    const base::android::JavaParamRef<jobject>& jprofile,
    JNIEnv* env,
    autofill::AutofillProfile* profile,
    const std::string& locale) {
  // Only set the guid if it is an existing profile (Java guid not empty).
  // Otherwise, keep the generated one.
  std::string guid = ConvertJavaStringToUTF8(
      Java_AssistantAutofillProfile_getGUID(env, jprofile));
  if (!guid.empty()) {
    profile->set_guid(guid);
  }

  profile->set_origin(ConvertJavaStringToUTF8(
      Java_AssistantAutofillProfile_getOrigin(env, jprofile)));
  MaybeSetInfo(profile, autofill::NAME_FULL,
               Java_AssistantAutofillProfile_getFullName(env, jprofile),
               locale);
  MaybeSetRawInfo(
      profile, autofill::NAME_HONORIFIC_PREFIX,
      Java_AssistantAutofillProfile_getHonorificPrefix(env, jprofile));
  MaybeSetRawInfo(profile, autofill::COMPANY_NAME,
                  Java_AssistantAutofillProfile_getCompanyName(env, jprofile));
  MaybeSetRawInfo(
      profile, autofill::ADDRESS_HOME_STREET_ADDRESS,
      Java_AssistantAutofillProfile_getStreetAddress(env, jprofile));
  MaybeSetRawInfo(profile, autofill::ADDRESS_HOME_STATE,
                  Java_AssistantAutofillProfile_getRegion(env, jprofile));
  MaybeSetRawInfo(profile, autofill::ADDRESS_HOME_CITY,
                  Java_AssistantAutofillProfile_getLocality(env, jprofile));
  MaybeSetRawInfo(
      profile, autofill::ADDRESS_HOME_DEPENDENT_LOCALITY,
      Java_AssistantAutofillProfile_getDependentLocality(env, jprofile));
  MaybeSetRawInfo(profile, autofill::ADDRESS_HOME_ZIP,
                  Java_AssistantAutofillProfile_getPostalCode(env, jprofile));
  MaybeSetRawInfo(profile, autofill::ADDRESS_HOME_SORTING_CODE,
                  Java_AssistantAutofillProfile_getSortingCode(env, jprofile));
  MaybeSetInfo(profile, autofill::ADDRESS_HOME_COUNTRY,
               Java_AssistantAutofillProfile_getCountryCode(env, jprofile),
               locale);
  MaybeSetRawInfo(profile, autofill::PHONE_HOME_WHOLE_NUMBER,
                  Java_AssistantAutofillProfile_getPhoneNumber(env, jprofile));
  MaybeSetRawInfo(profile, autofill::EMAIL_ADDRESS,
                  Java_AssistantAutofillProfile_getEmailAddress(env, jprofile));
  profile->set_language_code(ConvertJavaStringToUTF8(
      Java_AssistantAutofillProfile_getLanguageCode(env, jprofile)));
  profile->FinalizeAfterImport();
}

base::android::ScopedJavaLocalRef<jobject> CreateAssistantAutofillCreditCard(
    JNIEnv* env,
    const autofill::CreditCard& credit_card,
    const std::string& locale) {
  const autofill::data_util::PaymentRequestData& payment_request_data =
      autofill::data_util::GetPaymentRequestData(credit_card.network());
  return Java_AssistantAutofillCreditCard_Constructor(
      env, ConvertUTF8ToJavaString(env, credit_card.guid()),
      ConvertUTF8ToJavaString(env, credit_card.origin()),
      credit_card.record_type() == autofill::CreditCard::LOCAL_CARD,
      credit_card.record_type() == autofill::CreditCard::FULL_SERVER_CARD,
      ConvertUTF16ToJavaString(
          env, credit_card.GetRawInfo(autofill::CREDIT_CARD_NAME_FULL)),
      ConvertUTF16ToJavaString(
          env, credit_card.GetRawInfo(autofill::CREDIT_CARD_NUMBER)),
      ConvertUTF16ToJavaString(env, credit_card.NetworkAndLastFourDigits()),
      ConvertUTF16ToJavaString(
          env, credit_card.GetRawInfo(autofill::CREDIT_CARD_EXP_MONTH)),
      ConvertUTF16ToJavaString(
          env, credit_card.GetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR)),
      ConvertUTF8ToJavaString(env,
                              payment_request_data.basic_card_issuer_network),
      Java_AssistantAutofillCreditCard_getIssuerIconDrawableId(
          env, ConvertUTF8ToJavaString(
                   env, credit_card.CardIconStringForAutofillSuggestion())),
      ConvertUTF8ToJavaString(env, credit_card.billing_address_id()),
      ConvertUTF8ToJavaString(env, credit_card.server_id()),
      credit_card.instrument_id(),
      ConvertUTF16ToJavaString(env, credit_card.nickname()),
      url::GURLAndroid::FromNativeGURL(env, credit_card.card_art_url()),
      static_cast<jint>(credit_card.virtual_card_enrollment_state()),
      ConvertUTF16ToJavaString(env, credit_card.product_description()));
}

void PopulateAutofillCreditCardFromJava(
    const base::android::JavaParamRef<jobject>& jcredit_card,
    JNIEnv* env,
    autofill::CreditCard* credit_card,
    const std::string& locale) {
  // Only set the guid if it is an existing card (java guid not empty).
  // Otherwise, keep the generated one.
  std::string guid = ConvertJavaStringToUTF8(
      Java_AssistantAutofillCreditCard_getGUID(env, jcredit_card));
  if (!guid.empty()) {
    credit_card->set_guid(guid);
  }

  if (Java_AssistantAutofillCreditCard_getIsLocal(env, jcredit_card)) {
    credit_card->set_record_type(autofill::CreditCard::LOCAL_CARD);
  } else {
    if (Java_AssistantAutofillCreditCard_getIsCached(env, jcredit_card)) {
      credit_card->set_record_type(autofill::CreditCard::FULL_SERVER_CARD);
    } else {
      credit_card->set_record_type(autofill::CreditCard::MASKED_SERVER_CARD);
      credit_card->SetNetworkForMaskedCard(
          autofill::data_util::GetIssuerNetworkForBasicCardIssuerNetwork(
              ConvertJavaStringToUTF8(
                  env,
                  Java_AssistantAutofillCreditCard_getBasicCardIssuerNetwork(
                      env, jcredit_card))));
    }
  }

  credit_card->set_origin(ConvertJavaStringToUTF8(
      Java_AssistantAutofillCreditCard_getOrigin(env, jcredit_card)));
  credit_card->SetRawInfo(
      autofill::CREDIT_CARD_NAME_FULL,
      ConvertJavaStringToUTF16(
          Java_AssistantAutofillCreditCard_getName(env, jcredit_card)));
  credit_card->SetRawInfo(
      autofill::CREDIT_CARD_NUMBER,
      ConvertJavaStringToUTF16(
          Java_AssistantAutofillCreditCard_getNumber(env, jcredit_card)));
  credit_card->SetRawInfo(
      autofill::CREDIT_CARD_EXP_MONTH,
      ConvertJavaStringToUTF16(
          Java_AssistantAutofillCreditCard_getMonth(env, jcredit_card)));
  credit_card->SetRawInfo(
      autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR,
      ConvertJavaStringToUTF16(
          Java_AssistantAutofillCreditCard_getYear(env, jcredit_card)));
  credit_card->set_billing_address_id(ConvertJavaStringToUTF8(
      Java_AssistantAutofillCreditCard_getBillingAddressId(env, jcredit_card)));
  credit_card->set_server_id(ConvertJavaStringToUTF8(
      Java_AssistantAutofillCreditCard_getServerId(env, jcredit_card)));
  credit_card->set_instrument_id(
      Java_AssistantAutofillCreditCard_getInstrumentId(env, jcredit_card));
  credit_card->SetNickname(ConvertJavaStringToUTF16(
      Java_AssistantAutofillCreditCard_getNickname(env, jcredit_card)));
  base::android::ScopedJavaLocalRef<jobject> jcard_art_url =
      Java_AssistantAutofillCreditCard_getCardArtUrl(env, jcredit_card);
  if (!jcard_art_url.is_null()) {
    credit_card->set_card_art_url(
        *url::GURLAndroid::ToNativeGURL(env, jcard_art_url));
  }
  credit_card->set_virtual_card_enrollment_state(
      static_cast<autofill::CreditCard::VirtualCardEnrollmentState>(
          Java_AssistantAutofillCreditCard_getVirtualCardEnrollmentState(
              env, jcredit_card)));
  credit_card->set_product_description(ConvertJavaStringToUTF16(
      Java_AssistantAutofillCreditCard_getProductDescription(env,
                                                             jcredit_card)));
}

}  // namespace ui_controller_android_utils
}  // namespace autofill_assistant
