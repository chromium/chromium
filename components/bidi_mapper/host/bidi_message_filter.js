// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stands between a BidiMapperHost and the BiDi mapper that the host runs.
//
// The host evaluates this script in the mapper document before it evaluates
// the mapper there. The mapper is written to run behind ChromeDriver, which
// owns the BiDi session and trusts its client. This script does that part of
// the job:
//  - It handles `session.status`, `session.new` and `session.end`, which the
//    mapper leaves to its embedder, and lets other commands through only
//    while a session exists.
//  - It refuses the mapper's `goog:cdp` module, which gives raw access to the
//    DevTools protocol.
//  - It refuses `script.evaluate` and `script.callFunction`, which run script
//    of the client in the pages that the session automates.
// This script is the only place where the above is enforced.
//
// Messages of the client come in through `bidiHostOnMessage()`, which the
// host calls, and go on to `onBidiMessage()` of the mapper. Messages of the
// mapper come in through `sendBidiResponse()`, which the mapper calls, and go
// on to the host through a binding.

(function() {
'use strict';

// The bindings that the host added for this script, see BidiMapperHostImpl.
// They are taken away from the document, so that every message for the
// client passes through this script.
const sendToHost = window.bidiHostSendMessage;
const endSession = window.bidiHostEndSession;
delete window.bidiHostSendMessage;
delete window.bidiHostEndSession;
if (typeof sendToHost !== 'function' || typeof endSession !== 'function') {
  throw new Error('The bindings of the BiDi mapper host are missing');
}

// The module of the mapper that exposes the DevTools protocol. The names of
// its commands and events start with the name of the module.
const CDP_MODULE = 'goog:cdp';

// The commands that the client may not use. These two run script of the client
// in the pages that the session automates. They are an example.
// TODO(crbug.com/585298118): Most of the BiDi command surface is expected to
// be excluded. That will probably take an allowlist of the supported commands
// instead of this list.
const UNSUPPORTED_COMMANDS = new Set([
  'script.callFunction',
  'script.evaluate',
]);

// The mapper routes responses and events to the channel named by this field
// of a command.
const CHANNEL_KEY = 'goog:channel';

// The states of the BiDi session. A host serves one session at most.
const SessionState = {
  // No session was requested yet.
  NONE: 'none',
  // `session.new` was passed on to the mapper, which has not answered yet.
  CREATING: 'creating',
  ACTIVE: 'active',
  // The mapper failed to create the session. It cannot create another one.
  FAILED: 'failed',
  ENDED: 'ended',
};

let sessionState = SessionState.NONE;

// The id of the `session.new` command that the mapper is processing. Only set
// in the CREATING state.
let sessionNewCommandId = null;

// Returns the name of the JSON type of `value`, for error messages.
function getJsonType(value) {
  if (value === null) {
    return 'null';
  }
  if (Array.isArray(value)) {
    return 'array';
  }
  return typeof value;
}

function isObject(value) {
  return getJsonType(value) === 'object';
}

// Sends a response of the given type to the command with the given id and
// channel. `id` is null if the id of the command is not known.
function respond(type, id, channel, fields) {
  const response = {type, id, ...fields};
  // An empty channel is no channel.
  if (channel) {
    response[CHANNEL_KEY] = channel;
  }
  sendToHost(JSON.stringify(response));
}

function respondWithError(id, channel, error, message) {
  respond('error', id, channel, {error, message});
}

function respondWithSuccess(id, channel, result) {
  respond('success', id, channel, {result});
}

// Passes a command on to the mapper as the client sent it. The mapper parses
// it the same way this script did. Returns false if the mapper cannot take
// commands.
function sendToMapper(text) {
  if (typeof window.onBidiMessage !== 'function') {
    return false;
  }
  window.onBidiMessage(text);
  return true;
}

function respondThatMapperIsNotRunning(id, channel) {
  respondWithError(
      id, channel, 'unknown error', 'The BiDi mapper is not running');
}

// Returns what is wrong with the parameters of a `session.new` command, or
// null. The mapper does not validate them: it fails with an internal error
// for the shapes rejected here, and cannot create a session afterwards.
function validateSessionNewParams(params) {
  const capabilities = params.capabilities;
  if (!isObject(capabilities)) {
    return 'Expected object \'capabilities\'';
  }
  const alwaysMatch = capabilities.alwaysMatch;
  if (alwaysMatch !== undefined && !isObject(alwaysMatch)) {
    return 'Expected object \'capabilities.alwaysMatch\'';
  }
  const firstMatch = capabilities.firstMatch;
  if (firstMatch === undefined) {
    return null;
  }
  if (!Array.isArray(firstMatch)) {
    return 'Expected array \'capabilities.firstMatch\'';
  }
  if (!firstMatch.every(isObject)) {
    return 'Expected array of objects \'capabilities.firstMatch\'';
  }
  return null;
}

function handleSessionNew(command, channel, text) {
  const id = command.id;
  switch (sessionState) {
    case SessionState.NONE:
      break;
    case SessionState.CREATING:
    case SessionState.ACTIVE:
      respondWithError(
          id, channel, 'session not created', 'Session already exists');
      return;
    case SessionState.FAILED:
    case SessionState.ENDED:
      respondWithError(
          id, channel, 'session not created',
          'A new session requires a new connection');
      return;
    default:
      throw new Error(`Unexpected session state: ${sessionState}`);
  }

  const error = validateSessionNewParams(command.params);
  if (error) {
    respondWithError(id, channel, 'invalid argument', error);
    return;
  }

  sessionState = SessionState.CREATING;
  sessionNewCommandId = id;
  if (!sendToMapper(text)) {
    sessionState = SessionState.FAILED;
    sessionNewCommandId = null;
    respondThatMapperIsNotRunning(id, channel);
  }
}

// Returns the first event of the CDP module named by the parameters of a
// `session.subscribe` or `session.unsubscribe` command, if any.
function findCdpEvent(params) {
  if (!Array.isArray(params.events)) {
    return undefined;
  }
  return params.events.find(
      event => typeof event === 'string' && event.startsWith(CDP_MODULE));
}

// Handles a message of the client. Called by the host.
function onMessageFromClient(text) {
  // The mapper validates the envelope of a command as well. It is validated
  // here first, with the same errors, so that nothing below depends on what
  // the mapper does with a malformed command.
  let command;
  try {
    command = JSON.parse(text);
  } catch {
    respondWithError(
        null, null, 'invalid argument', 'Cannot parse data as JSON');
    return;
  }
  if (!isObject(command)) {
    respondWithError(
        null, null, 'invalid argument',
        `Expected JSON object but got ${getJsonType(command)}`);
    return;
  }

  const {id, method, params} = command;
  if (!Number.isSafeInteger(id) || id < 0) {
    respondWithError(
        null, null, 'invalid argument',
        `Expected unsigned integer but got ${getJsonType(id)}`);
    return;
  }
  if (typeof method !== 'string') {
    respondWithError(
        id, null, 'invalid argument',
        `Expected string method but got ${getJsonType(method)}`);
    return;
  }
  if (!isObject(params)) {
    respondWithError(
        id, null, 'invalid argument',
        `Expected object params but got ${getJsonType(params)}`);
    return;
  }
  const channel = command[CHANNEL_KEY];
  if (channel !== undefined && typeof channel !== 'string') {
    respondWithError(
        id, null, 'invalid argument',
        `Expected string channel but got ${getJsonType(channel)}`);
    return;
  }

  // The CDP module does not exist for the client.
  if (method.startsWith(CDP_MODULE)) {
    respondWithError(
        id, channel, 'unknown command', `Unknown command '${method}'.`);
    return;
  }

  if (method === 'session.status') {
    const ready = sessionState === SessionState.NONE;
    respondWithSuccess(id, channel, {
      ready,
      message: ready ? 'ready for a new session' :
                       'a new session requires a new connection',
    });
    return;
  }

  if (method === 'session.new') {
    handleSessionNew(command, channel, text);
    return;
  }

  if (sessionState !== SessionState.ACTIVE) {
    respondWithError(id, channel, 'invalid session id', 'No active session');
    return;
  }

  if (method === 'session.end') {
    sessionState = SessionState.ENDED;
    respondWithSuccess(id, channel, {});
    // The host closes once it has passed the response on.
    endSession('');
    return;
  }

  if (UNSUPPORTED_COMMANDS.has(method)) {
    respondWithError(
        id, channel, 'unsupported operation',
        `Method ${method} is not supported.`);
    return;
  }

  if (method === 'session.subscribe' || method === 'session.unsubscribe') {
    const event = findCdpEvent(params);
    if (event !== undefined) {
      respondWithError(
          id, channel, 'invalid argument', `Unknown event: ${event}`);
      return;
    }
  }

  if (!sendToMapper(text)) {
    respondThatMapperIsNotRunning(id, channel);
  }
}

// Handles a message of the mapper. Called by the mapper.
function onMessageFromMapper(text) {
  if (typeof text !== 'string') {
    return;
  }

  // A message has to be parsed in two cases only: when it may be the response
  // to `session.new`, and when it may be an event of the CDP module. The name
  // of the module is written to JSON as is, so a message that does not
  // contain it is not such an event.
  const mayBeCdpEvent = text.includes(CDP_MODULE);
  if (sessionState !== SessionState.CREATING && !mayBeCdpEvent) {
    sendToHost(text);
    return;
  }

  let message;
  try {
    message = JSON.parse(text);
  } catch {
    message = undefined;
  }
  if (!isObject(message)) {
    // The message cannot be inspected. Do not let it through if it may be an
    // event of the CDP module.
    if (!mayBeCdpEvent) {
      sendToHost(text);
    }
    return;
  }

  // No client can subscribe to the events of the CDP module, so the mapper is
  // not expected to send any. Drop them regardless.
  if (typeof message.method === 'string' &&
      message.method.startsWith(CDP_MODULE)) {
    return;
  }

  if (sessionState !== SessionState.CREATING ||
      message.id !== sessionNewCommandId) {
    sendToHost(text);
    return;
  }

  // This is the response to `session.new`.
  sessionNewCommandId = null;
  if (message.type !== 'success' || !isObject(message.result)) {
    sessionState = SessionState.FAILED;
    sendToHost(text);
    return;
  }
  sessionState = SessionState.ACTIVE;
  // The mapper does not know the id of the session: its embedder owns it.
  message.result.sessionId = crypto.randomUUID();
  sendToHost(JSON.stringify(message));
}

// Makes `value` available to the host or to the mapper under `name`. A script
// that runs later, the mapper included, cannot put anything else there.
function expose(name, value) {
  Object.defineProperty(
      window, name, {value, writable: false, configurable: false});
}

expose('bidiHostOnMessage', onMessageFromClient);
expose('sendBidiResponse', onMessageFromMapper);
})();
