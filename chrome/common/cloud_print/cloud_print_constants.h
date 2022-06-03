// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CLOUD_PRINT_CLOUD_PRINT_CONSTANTS_H_
#define CHROME_COMMON_CLOUD_PRINT_CLOUD_PRINT_CONSTANTS_H_


namespace cloud_print {

// The string to be appended to the user-agent for cloud print requests.
extern const char kCloudPrintUserAgent[];
// The proxy header name required by cloud print server.
extern const char kChromeCloudPrintProxyHeaderName[];
// The proxy header value required by cloud print server.
extern const char kChromeCloudPrintProxyHeaderValue[];
// The source of cloud print notifications.
extern const char kCloudPrintPushNotificationsSource[];

// Values used to register or update a printer with the cloud print service.
extern const char kProxyIdValue[];
extern const char kPrinterNameValue[];
extern const char kPrinterDescValue[];
extern const char kPrinterCapsValue[];
extern const char kPrinterDisplayNameValue[];
extern const char kPrinterDefaultsValue[];
extern const char kPrinterStatusValue[];
extern const char kPrinterTagValue[];
extern const char kPrinterRemoveTagValue[];
extern const char kPrinterLocalSettingsValue[];
extern const char kMessageTextValue[];
extern const char kUseCDD[];

extern const char kContentTypeJSON[];
extern const char kContentTypePDF[];

// Value of "code" parameter in cloud print "/message" requests.
extern const char kPrintSystemFailedMessageId[];
extern const char kGetPrinterCapsFailedMessageId[];
extern const char kEnumPrintersFailedMessageId[];
extern const char kZombiePrinterMessageId[];

// Values in the respone JSON from the cloud print server.
extern const char kSuccessValue[];
extern const char kNameValue[];
extern const char kDisplayNameValue[];
extern const char kIdValue[];
extern const char kTicketUrlValue[];
extern const char kFileUrlValue[];
extern const char kPrinterListValue[];
extern const char kJobListValue[];
extern const char kTitleValue[];
extern const char kOwnerValue[];
extern const char kPrinterCapsHashValue[];
extern const char kTagsValue[];
extern const char kXMPPJidValue[];
extern const char kOAuthCodeValue[];
extern const char kCreateTimeValue[];
extern const char kPrinterTypeValue[];
extern const char kUserValue[];
extern const char kUsersValue[];
extern const char kLocalSettingsPendingXmppValue[];

// Value in XMPP notification.
extern const char kNotificationUpdateSettings[];

// Printer tag names. Don't need prefixes. They will be added on submit.
extern const char kChromeVersionTagName[];
extern const char kSystemNameTagName[];
extern const char kSystemVersionTagName[];

// Tags for cloud print service.
extern const char kCloudPrintServiceProxyTagPrefix[];
extern const char kCloudPrintServiceTagsHashTagName[];
extern const char kCloudPrintServiceTagDryRunFlag[];

// Reasons for fetching print jobs.
// Job fetch on proxy startup.
extern const char kJobFetchReasonStartup[];
// Job fetch because we are polling.
extern const char kJobFetchReasonPoll[];
// Job fetch on being notified by the server.
extern const char kJobFetchReasonNotified[];
// Job fetch after a successful print to query for more jobs.
extern const char kJobFetchReasonQueryMore[];
// Job fetch after a job failure to query for more jobs.
extern const char kJobFetchReasonFailure[];
// Job fetch due to scheduled retry.
extern const char kJobFetchReasonRetry[];

// Format of the local settings containing only XMPP ping.
extern const char kCreateLocalSettingsXmppPingFormat[];
extern const char kUpdateLocalSettingsXmppPingFormat[];

// Max retry count for job data fetch requests.
const int kJobDataMaxRetryCount = 1;
// Max retry count (infinity) for API fetch requests.
const int kCloudPrintAPIMaxRetryCount = -1;
// Max retry count (infinity) for Registration requests.
const int kCloudPrintRegisterMaxRetryCount = -1;
// Max retry count (infinity) for authentication requests.
const int kCloudPrintAuthMaxRetryCount = -1;

// When we don't have XMPP notifications available, we resort to polling for
// print jobs. We choose a random interval in seconds between these 2 values.
const int kMinJobPollIntervalSecs = 5*60;  // 5 minutes in seconds
const int kMaxJobPollIntervalSecs = 8*60;  // 8 minutes in seconds

// When we have XMPP notifications available, we ping server to keep connection
// alive or check connection status.
const int kDefaultXmppPingTimeoutSecs = 5*60;
const int kMinXmppPingTimeoutSecs = 1*60;
const int kXmppPingCheckIntervalSecs = 60;

// Number of failed pings before we try to reinstablish XMPP connection.
const int kMaxFailedXmppPings = 2;

// The number of seconds before the OAuth2 access token is due to expire that
// we try and refresh it.
const int kTokenRefreshGracePeriodSecs = 5*60;  // 5 minutes in seconds

// The number of retries before we abandon a print job in exponential backoff
const int kNumRetriesBeforeAbandonJob = 5;

// The wait time for the second (first with wait time) retry for a job that
// fails due to network errors
const int kJobFirstWaitTimeSecs = 1;

// The multiplier for the wait time for retrying a job that fails due to
// network errors
const int kJobWaitTimeExponentialMultiplier = 2;

}  // namespace cloud_print

#endif  // CHROME_COMMON_CLOUD_PRINT_CLOUD_PRINT_CONSTANTS_H_
