// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_IOS_UKM_REPORTING_IOS_UTIL_H_
#define COMPONENTS_UKM_IOS_UKM_REPORTING_IOS_UTIL_H_

// Utilities in this file should help to figure out the root cause of
// crbug.com/1154678 (Data loss on UMA in iOS). If total sum of
// "UKM.IOSLog.OnSuccess" is greater than total count of "UKM.LogSize.OnSuccess"
// records, then data loss is caused by failure to write the histogram to the
// disk on background thread. Otherwise (if total sum of
// "UKM.IOSLog.OnSuccess" is equal to total count of "UKM.LogSize.OnSuccess"
// records) then there is actually no data loss and app simply gets terminated
// in a short window between UKM reached the server and the API call which
// records the data.

// Records "UKM.IOSLog.OnSuccess" histogram and resets the counter.
void RecordAndResetUkmLogSizeOnSuccessCounter();

// Increments the counter which will be recorded as UMA histogram when
// RecordAndResetUkmLogSizeOnSuccessCounter is called. Counter represents
// number of times "UKM.LogSize.OnSuccess" was recorded.
void IncrementUkmLogSizeOnSuccessCounter();

#endif  // COMPONENTS_UKM_IOS_UKM_REPORTING_IOS_UTIL_H_
