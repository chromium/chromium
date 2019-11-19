// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_DATA_TYPE_HISTOGRAM_H_
#define COMPONENTS_SYNC_BASE_DATA_TYPE_HISTOGRAM_H_

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/sync/base/model_type.h"

// Converts memory size |bytes| into kilobytes and records it into |model_type|
// related histogram for memory footprint of sync data.
void SyncRecordModelTypeMemoryHistogram(syncer::ModelType model_type,
                                        size_t bytes);

// Records |count| into a |model_type| related histogram for count of sync
// entities.
void SyncRecordModelTypeCountHistogram(syncer::ModelType model_type,
                                       size_t count);

// Helper macro for datatype specific histograms. For each datatype, invokes
// a pre-defined PER_DATA_TYPE_MACRO(type_str), where |type_str| is the string
// version of the datatype.
//
// Example usage (ignoring newlines necessary for multiline macro):
// std::vector<syncer::ModelType> types = GetEntryTypes();
// for (int i = 0; i < types.size(); ++i) {
// #define PER_DATA_TYPE_MACRO(type_str)
//     UMA_HISTOGRAM_ENUMERATION("Sync." type_str "StartFailures",
//                               error, max_error);
//   SYNC_DATA_TYPE_HISTOGRAM(types[i]);
// #undef PER_DATA_TYPE_MACRO
// }
//
// TODO(zea): Once visual studio supports proper variadic argument replacement
// in macros, pass in the histogram method directly as a parameter.
// See http://connect.microsoft.com/VisualStudio/feedback/details/380090/
// variadic-macro-replacement#details
// When adding a new datatype in the switch below, also update the SyncModelType
// and SyncModelTypeByMacro histogram suffixes in histograms.xml.
#define SYNC_DATA_TYPE_HISTOGRAM(datatype)                     \
  do {                                                         \
    switch (datatype) {                                        \
      case ::syncer::BOOKMARKS:                                \
        PER_DATA_TYPE_MACRO("Bookmarks");                      \
        break;                                                 \
      case ::syncer::PREFERENCES:                              \
        PER_DATA_TYPE_MACRO("Preferences");                    \
        break;                                                 \
      case ::syncer::PASSWORDS:                                \
        PER_DATA_TYPE_MACRO("Passwords");                      \
        break;                                                 \
      case ::syncer::AUTOFILL_PROFILE:                         \
        PER_DATA_TYPE_MACRO("AutofillProfiles");               \
        break;                                                 \
      case ::syncer::AUTOFILL:                                 \
        PER_DATA_TYPE_MACRO("Autofill");                       \
        break;                                                 \
      case ::syncer::AUTOFILL_WALLET_DATA:                     \
        PER_DATA_TYPE_MACRO("AutofillWallet");                 \
        break;                                                 \
      case ::syncer::AUTOFILL_WALLET_METADATA:                 \
        PER_DATA_TYPE_MACRO("AutofillWalletMetadata");         \
        break;                                                 \
      case ::syncer::THEMES:                                   \
        PER_DATA_TYPE_MACRO("Themes");                         \
        break;                                                 \
      case ::syncer::TYPED_URLS:                               \
        PER_DATA_TYPE_MACRO("TypedUrls");                      \
        break;                                                 \
      case ::syncer::EXTENSIONS:                               \
        PER_DATA_TYPE_MACRO("Extensions");                     \
        break;                                                 \
      case ::syncer::SEARCH_ENGINES:                           \
        PER_DATA_TYPE_MACRO("SearchEngines");                  \
        break;                                                 \
      case ::syncer::SESSIONS:                                 \
        PER_DATA_TYPE_MACRO("Sessions");                       \
        break;                                                 \
      case ::syncer::APPS:                                     \
        PER_DATA_TYPE_MACRO("Apps");                           \
        break;                                                 \
      case ::syncer::APP_SETTINGS:                             \
        PER_DATA_TYPE_MACRO("AppSettings");                    \
        break;                                                 \
      case ::syncer::EXTENSION_SETTINGS:                       \
        PER_DATA_TYPE_MACRO("ExtensionSettings");              \
        break;                                                 \
      case ::syncer::HISTORY_DELETE_DIRECTIVES:                \
        PER_DATA_TYPE_MACRO("HistoryDeleteDirectives");        \
        break;                                                 \
      case ::syncer::DICTIONARY:                               \
        PER_DATA_TYPE_MACRO("Dictionary");                     \
        break;                                                 \
      case ::syncer::FAVICON_IMAGES:                           \
        PER_DATA_TYPE_MACRO("FaviconImages");                  \
        break;                                                 \
      case ::syncer::FAVICON_TRACKING:                         \
        PER_DATA_TYPE_MACRO("FaviconTracking");                \
        break;                                                 \
      case ::syncer::DEVICE_INFO:                              \
        PER_DATA_TYPE_MACRO("DeviceInfo");                     \
        break;                                                 \
      case ::syncer::PRIORITY_PREFERENCES:                     \
        PER_DATA_TYPE_MACRO("PriorityPreferences");            \
        break;                                                 \
      case ::syncer::SUPERVISED_USER_SETTINGS:                 \
        PER_DATA_TYPE_MACRO("ManagedUserSetting");             \
        break;                                                 \
      case ::syncer::APP_LIST:                                 \
        PER_DATA_TYPE_MACRO("AppList");                        \
        break;                                                 \
      case ::syncer::SUPERVISED_USER_WHITELISTS:               \
        PER_DATA_TYPE_MACRO("ManagedUserWhitelist");           \
        break;                                                 \
      case ::syncer::ARC_PACKAGE:                              \
        PER_DATA_TYPE_MACRO("ArcPackage");                     \
        break;                                                 \
      case ::syncer::PRINTERS:                                 \
        PER_DATA_TYPE_MACRO("Printers");                       \
        break;                                                 \
      case ::syncer::READING_LIST:                             \
        PER_DATA_TYPE_MACRO("ReadingList");                    \
        break;                                                 \
      case ::syncer::USER_CONSENTS:                            \
        PER_DATA_TYPE_MACRO("UserConsents");                   \
        break;                                                 \
      case ::syncer::USER_EVENTS:                              \
        PER_DATA_TYPE_MACRO("UserEvents");                     \
        break;                                                 \
      case ::syncer::PROXY_TABS:                               \
        PER_DATA_TYPE_MACRO("Tabs");                           \
        break;                                                 \
      case ::syncer::NIGORI:                                   \
        PER_DATA_TYPE_MACRO("Nigori");                         \
        break;                                                 \
      case ::syncer::DEPRECATED_EXPERIMENTS:                   \
        PER_DATA_TYPE_MACRO("Experiments");                    \
        break;                                                 \
      case ::syncer::SEND_TAB_TO_SELF:                         \
        PER_DATA_TYPE_MACRO("SendTabToSelf");                  \
        break;                                                 \
      case ::syncer::SECURITY_EVENTS:                          \
        PER_DATA_TYPE_MACRO("SecurityEvents");                 \
        break;                                                 \
      case ::syncer::WEB_APPS:                                 \
        PER_DATA_TYPE_MACRO("WebApps");                        \
        break;                                                 \
      case ::syncer::WIFI_CONFIGURATIONS:                      \
        PER_DATA_TYPE_MACRO("WifiConfigurations");             \
        break;                                                 \
      case ::syncer::OS_PREFERENCES:                           \
        PER_DATA_TYPE_MACRO("OsPreferences");                  \
        break;                                                 \
      case ::syncer::OS_PRIORITY_PREFERENCES:                  \
        PER_DATA_TYPE_MACRO("OsPriorityPreferences");          \
        break;                                                 \
      default:                                                 \
        NOTREACHED() << "Unknown datatype "                    \
                     << ::syncer::ModelTypeToString(datatype); \
    }                                                          \
  } while (0)

#endif  // COMPONENTS_SYNC_BASE_DATA_TYPE_HISTOGRAM_H_
