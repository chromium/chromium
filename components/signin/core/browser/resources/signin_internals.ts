// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';
// </if>

import '/strings.m.js';

import {html, render} from '//resources/lit/v3_0/lit.rollup.js';
import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

interface CookieInfo {
  email: string;
  gaia_id: string;
  valid: string;
}

interface CookieAccountsInfo {
  cookie_info: CookieInfo[];
}

interface AccountInfo {
  accountId: string;
  hasRefreshToken?: boolean;
  hasAuthError?: boolean;
  isBound?: boolean;
}

interface RefreshTokenEvent {
  accountId: string;
  timestamp: string;
  type: string;
  source: string;
}

interface BoundSessionInfo {
  sessionID: string;
  domain?: string;
  path?: string;
  expirationTime?: string;
  throttlingPaused?: boolean;
  boundCookieNames?: string;
  refreshUrl?: string;
}

interface BasicInfoData {
  label: string;
  status: string;
  time: string;
}

interface BasicInfo {
  title: string;
  data: BasicInfoData[];
}

interface TokenInfoData {
  service: string;
  scopes: string;
  request_time: string;
  status: string;
}

interface TokenInfo {
  title: string;
  data: TokenInfoData[];
}

interface SigninInfo {
  accountInfo: AccountInfo[];
  refreshTokenEvents: RefreshTokenEvent[];
  boundSessionInfo?: BoundSessionInfo[];
  signin_info: BasicInfo[];
  token_info: TokenInfo[];
}

function getSigninInfoHtml(infos: BasicInfo[]) {
  // clang-format off
  return html`
    ${infos.map(item => html`
      <div class="section">
        <h2>${item.title}</h2>
        <table class="signin-details">
          ${item.data.map(data => html`
            <tr>
              <td>${data.label}</td>
              <td>${data.status}</td>
              <td ?hidden="${!data.time}">${data.time}</td>
              <td ?hidden="${data.time.length!==0}">&nbsp;</td>
            </tr>
          `)}
        </table>
      </div>
    `)}
  `;
  // clang-format on
}

function getTokenInfoHtml(infos: TokenInfo[]) {
  // clang-format off
  return html`
    <h2>Access Token Details By Account</h2>
    ${infos.map(item => html`
      <div class="tokenSection">
        <h3>${item.title}</h3>
        <table class="signin-details">
          <tr class="header">
            <td>Service</td>
            <td>Requested Scopes</td>
            <td>Request Time</td>
            <td>Request Status</td>
          </tr>
          ${item.data.map(data => html`
            <tr class="${getClassFromValue(data.status)}">
              <td>${data.service}</td>
              <td>${data.scopes}</td>
              <td>${data.request_time}</td>
              <td style="${data.status.includes('Expired at')
                         ? 'color: #ffffff; background-color: #ff0000' : ''}">
                ${data.status}
              </td>
            </tr>
         `)}
        </table>
      </div>
    `)}
  `;
  // clang-format on
}

function getCookieInfoHtml(cookieAccountsInfo: CookieAccountsInfo) {
  // clang-format off
  return html`
    <h2>Accounts in Cookie Jar</h2>
    <div class="cookieSection">
      <table class="signin-details">
        <tr class="header">
          <td>Email Address</td>
          <td>Gaia ID</td>
          <td>Validity</td>
        </tr>
        ${cookieAccountsInfo.cookie_info.map(item => html`
          <tr>
            <td>${item.email}</td>
            <td>${item.gaia_id}</td>
            <td>${item.valid}</td>
          </tr>
        `)}
      </table>
    </div>
  `;
  // clang-format on
}

function getAccountInfoHtml(infos: AccountInfo[]) {
  // clang-format off
  return html`
    <h2>Accounts in Token Service</h2>
    <div class="account-section">
      <table class="signin-details">
        <tr class="header">
          <td>Account Id</td>
          <td>Has refresh token</td>
          <td>Has persistent auth error</td>
          <td ?hidden="${infos[0]!.isBound == null}">Is bound to the device</td>
        </tr>
        ${infos.map(item => html`
          <tr>
            <td>${item.accountId}</td>
            <td>${item.hasRefreshToken}</td>
            <td>${item.hasAuthError}</td>
            <td ?hidden="${item.isBound == null}">${item.isBound}</td>
          </tr>
        `)}
      </table>
    </div>
  `;
  // clang-format on
}

function getRefreshTokenEventsHtml(events: RefreshTokenEvent[]) {
  // clang-format off
  return html`
    <h2>Refresh token events</h2>
    <div class="refresh-token-events-section">
      <table class="signin-details">
        <tr class="header">
          <td>Timestamp</td>
          <td>Accound Id</td>
          <td>Event type</td>
          <td>Source</td>
        </tr>
        ${events.map(event=> html`
          <tr>
            <td>${event.timestamp}</td>
            <td>${event.accountId}</td>
            <td>${event.type}</td>
            <td>${event.source}</td>
          </tr>
        `)}
      </table>
    </div>
  `;
  // clang-format on
}

function getBoundSessionInfoHtml(infos?: BoundSessionInfo[]) {
  if (!infos) {
    return html``;
  }

  // clang-format off
  return html`
    <div id="bound-session-info"">
      <h2>Bound sessions</h2>
      <div>
        <table class="signin-details">
          <tr class="header">
            <td>Session ID</td>
            <td>Domain</td>
            <td>Path</td>
            <td>Expiration time</td>
            <td>Throttling Paused</td>
            <td>Bound Cookie Names</td>
            <td>Refresh URL</td>
          </tr>
          ${infos.map(item => html`
            <tr>
              <td>${item.sessionID}</td>
              <td>${item.domain}</td>
              <td>${item.path}</td>
              <td style="${item.expirationTime &&
                           item.expirationTime.includes('Expired at')
                         ? 'color: #ffffff; background-color: #ff0000' : ''}">
                ${item.expirationTime}
              </td>
              <td>${item.throttlingPaused}</td>
              <td>${item.boundCookieNames}</td>
              <td>${item.refreshUrl}</td>
            </tr>
          `)}
        </table>
      </div>
    </div>
  `;
  // clang-format on
}

function getClassFromValue(value: string): string {
  if (value === 'Successful') {
    return 'ok';
  }

  return '';
}

// Replace the displayed values with the latest fetched ones.
function refreshSigninInfo(signinInfo: SigninInfo) {
  // Process templates even against an empty `signinInfo` to hide some sections.
  render(
      getSigninInfoHtml(signinInfo.signin_info),
      getRequiredElement('signin-info'));
  render(
      getTokenInfoHtml(signinInfo.token_info),
      getRequiredElement('token-info'));
  render(
      getAccountInfoHtml(signinInfo.accountInfo),
      getRequiredElement('account-info'));
  render(
      getRefreshTokenEventsHtml(signinInfo.refreshTokenEvents),
      getRequiredElement('refresh-token-events'));
  render(
      getBoundSessionInfoHtml(signinInfo.boundSessionInfo),
      getRequiredElement('bound-session-info'));
}

// Replace the cookie information with the fetched values.
function updateCookieAccounts(info: CookieAccountsInfo) {
  render(getCookieInfoHtml(info), getRequiredElement('cookie-info'));
}

// On load, do an initial refresh and register refreshSigninInfo to be invoked
// whenever we get new signin information from SigninInternalsUI.
function onLoad() {
  addWebUiListener('signin-info-changed', refreshSigninInfo);
  addWebUiListener('update-cookie-accounts', updateCookieAccounts);

  sendWithPromise('getSigninInfo').then(refreshSigninInfo);
}

document.addEventListener('DOMContentLoaded', onLoad);
