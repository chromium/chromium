// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}

function reportResult(auctionConfig, browserSignals) {
  sendReportTo(browserSignals.interestGroupOwner + '/echo?decision_report');
  return 'signalsForWinner';
}
