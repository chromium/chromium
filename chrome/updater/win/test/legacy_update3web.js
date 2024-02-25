// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is a script that allows running an updater client that can use the
// functionality provided by the legacy on-demand interfaces `IGoogleUpdate3Web`
// and `IAppCommandWeb`.
//
// Usage:
// ```
//   cscript legacy_update3web.js {0|1==Chromium|Chrome Branded} {AppId}
//       {is_system: 0|1} {0|1|2|3==CheckForUpdate|Install|Update|LaunchCommand}
//       {command_id?}`
// ```
//
// For instance, to check for Chrome updates, here is the command line and
// sample output:
// ```
//   cscript legacy_update3web.js 1 {8A69D345-D564-463c-AFF1-A69D9E530F96} 1 0
//   Microsoft (R) Windows Script Host Version 5.812
//   Copyright (C) Microsoft Corporation. All rights reserved.
//
//   [State][3][Checking for update...]
//   [Data][[Current Version][111.0.5563.65]]
//   [State][3][Checking for update...]
//   [Data][[Current Version][111.0.5563.65]]
//   [State][16][No update available!]
// ```
//
// To launch a command registered as `test-command-exe` on `Chromium` AppId
// `{23E39345-8B3C-4DF4-9CA6-53EA97B6C7A9}`, here is the command line and sample
// output:
// ```
//   cscript legacy_update3web.js 0 {23E39345-8B3C-4DF4-9CA6-53EA97B6C7A9} 1 3
//       test-command-exe
//   Microsoft (R) Windows Script Host Version 5.812
//   Copyright (C) Microsoft Corporation. All rights reserved.
//
//   Launching command.
//   Command launched.
//   [State][2][Running...]
//   [State][4][Exited with code 23.]
// ```

// Operations.
var CHECK_FOR_UPDATE = 0;
var INSTALL = 1;
var UPDATE = 2;
var LAUNCH_COMMAND = 3;

// The following states need to be kept in sync with the CurrentState enum in
// omaha3_idl.idl.
var STATE_INIT = 1;
var STATE_WAITING_TO_CHECK_FOR_UPDATE = STATE_INIT + 1;
var STATE_CHECKING_FOR_UPDATE = STATE_WAITING_TO_CHECK_FOR_UPDATE + 1;
var STATE_UPDATE_AVAILABLE = STATE_CHECKING_FOR_UPDATE + 1;
var STATE_WAITING_TO_DOWNLOAD = STATE_UPDATE_AVAILABLE + 1;
var STATE_RETRYING_DOWNLOAD = STATE_WAITING_TO_DOWNLOAD + 1;
var STATE_DOWNLOADING = STATE_RETRYING_DOWNLOAD + 1;
var STATE_DOWNLOAD_COMPLETE = STATE_DOWNLOADING + 1;
var STATE_EXTRACTING = STATE_DOWNLOAD_COMPLETE + 1;
var STATE_APPLYING_DIFFERENTIAL_PATCH = STATE_EXTRACTING + 1;
var STATE_READY_TO_INSTALL = STATE_APPLYING_DIFFERENTIAL_PATCH + 1;
var STATE_WAITING_TO_INSTALL = STATE_READY_TO_INSTALL + 1;
var STATE_INSTALLING = STATE_WAITING_TO_INSTALL + 1;
var STATE_INSTALL_COMPLETE = STATE_INSTALLING + 1;
var STATE_PAUSED = STATE_INSTALL_COMPLETE + 1;
var STATE_NO_UPDATE = STATE_PAUSED + 1;
var STATE_ERROR = STATE_NO_UPDATE + 1;

// The following states need to be kept in sync with the AppCommandStatus enum
// in omaha3_idl.idl.
var COMMAND_STATUS_INIT = 1;
var COMMAND_STATUS_RUNNING = COMMAND_STATUS_INIT + 1;
var COMMAND_STATUS_ERROR = COMMAND_STATUS_RUNNING + 1;
var COMMAND_STATUS_COMPLETE = COMMAND_STATUS_ERROR + 1;

function update3webProgId(is_chrome_branded, is_system) {
  return (is_chrome_branded ? 'Google' : 'Chromium') + 'Update.Update3Web' +
         (is_system ? 'Machine' : 'User');
}

function initializeBundle(is_chrome_branded, is_system) {
  var update3web = WScript.CreateObject(update3webProgId(is_chrome_branded,
                                                         is_system));
  var bundle = update3web.createAppBundleWeb();
  bundle.initialize();
  return bundle;
}

function initializeBundleForInstall(is_chrome_branded, is_system) {
  return initializeBundle(is_chrome_branded, is_system);
}

function doCheckForUpdate(is_chrome_branded, appId, is_system) {
  var bundle = initializeBundle(is_chrome_branded, is_system);

  var app = bundle.createInstalledApp(appId);
  bundle.checkForUpdate();
  doLoopUntilDone(CHECK_FOR_UPDATE, bundle);

  app = null;
  bundle = null;
  CollectGarbage();
}

function doInstall(is_chrome_branded, appId, is_system) {
  var bundle = initializeBundleForInstall(is_chrome_branded, is_system);

  bundle.createApp(appId, 'GPEZ', 'en', '');
  bundle.checkForUpdate();
  doLoopUntilDone(INSTALL, bundle);
}

function doUpdate(is_chrome_branded, appId, is_system) {
  var bundle = initializeBundleForInstall(is_chrome_branded, is_system);

  bundle.createInstalledApp(appId);
  bundle.checkForUpdate();
  doLoopUntilDone(UPDATE, bundle);
}

function doLaunchCommand(is_chrome_branded,
                         appId,
                         is_system,
                         command,
                         argument_list) {
  var bundle = initializeBundle(is_chrome_branded, is_system);
  bundle.createInstalledApp(appId);

  var app = bundle.appWeb(0);
  if (!app) {
    WScript.Echo('App not found.');
    return;
  }

  var cmd = app.command(command);
  if (!cmd) {
    WScript.Echo('Command not found.');
    return;
  }

  try {
    WScript.Echo('Launching command.');

    switch (argument_list.length) {
      case 0:
        cmd.execute();
        break;
      case 1:
        cmd.execute(argument_list[0]);
        break;
      case 2:
        cmd.execute(argument_list[0], argument_list[1]);
        break;
      case 3:
        cmd.execute(argument_list[0], argument_list[1], argument_list[2]);
        break;
      case 4:
        cmd.execute(argument_list[0],
                    argument_list[1],
                    argument_list[2],
                    argument_list[3]);
        break;
      case 5:
        cmd.execute(argument_list[0],
                    argument_list[1],
                    argument_list[2],
                    argument_list[3],
                    argument_list[4]);
      case 6:
        cmd.execute(argument_list[0],
                    argument_list[1],
                    argument_list[2],
                    argument_list[3],
                    argument_list[4],
                    argument_list[5]);
      case 7:
        cmd.execute(argument_list[0],
                    argument_list[1],
                    argument_list[2],
                    argument_list[3],
                    argument_list[4],
                    argument_list[5],
                    argument_list[6]);
      case 8:
        cmd.execute(argument_list[0],
                    argument_list[1],
                    argument_list[2],
                    argument_list[3],
                    argument_list[4],
                    argument_list[5],
                    argument_list[6],
                    argument_list[7]);
      case 9:
        cmd.execute(argument_list[0],
                    argument_list[1],
                    argument_list[2],
                    argument_list[3],
                    argument_list[4],
                    argument_list[5],
                    argument_list[6],
                    argument_list[7],
                    argument_list[8]);
        break;
      default: WScript.Echo('Too many arguments.'); return;
    }
    WScript.Echo('Command launched.');
  } catch (ex) {
    WScript.Echo('Error: ' + ex.description + ': ' + ex.number);
    return;
  }

  while (true) {
    var status = cmd.status;
    var stateDescription = '';

    switch (status) {
      case COMMAND_STATUS_INIT:
        stateDescription = 'Initializing...';
        break;

      case COMMAND_STATUS_RUNNING:
        stateDescription = 'Running...';
        break;

      case COMMAND_STATUS_ERROR:
        stateDescription = 'Error!';
        break;

      case COMMAND_STATUS_COMPLETE:
        stateDescription = 'Exited with code ' + cmd.exitCode + '.';
        break;

      default:
        stateDescription = 'Unhandled state!';
        break;
    }
    WScript.Echo('[State][' + status + '][' + stateDescription + ']');

    if (status != COMMAND_STATUS_RUNNING) {
      return;
    }
    WScript.Sleep(100);
  }
}

function doLoopUntilDone(operation, bundle) {
  function operationDone() {
    if (!bundle) {
      WScript.Echo('No bundle is defined!');
      return false;
    }

    var done = false;
    var app = bundle.appWeb(0);
    var state = app.currentState;
    var stateDescription = '';
    var extraData = '';

    switch (state.stateValue) {
      case STATE_INIT:
        stateDescription = 'Initializating...';
        break;

      case STATE_WAITING_TO_CHECK_FOR_UPDATE:
      case STATE_CHECKING_FOR_UPDATE:
        stateDescription = 'Checking for update...';
        extraData = '[Current Version][' + app.currentVersionWeb.version + ']';
        break;

      case STATE_UPDATE_AVAILABLE:
        stateDescription = 'Update available!';
        extraData = '[Next Version][' + app.nextVersionWeb.version + ']';
        if (operation == CHECK_FOR_UPDATE) {
          done = true;
          break;
        }

        bundle.download();
        break;

      case STATE_WAITING_TO_DOWNLOAD:
      case STATE_RETRYING_DOWNLOAD:
        stateDescription = 'Contacting server...';
        break;

      case STATE_DOWNLOADING:
        stateDescription = 'Downloading...';
        extraData = '[Bytes downloaded][' + state.bytesDownloaded + ']' +
                    '[Bytes total][' + state.totalBytesToDownload + ']' +
                    '[Time remaining][' + state.downloadTimeRemainingMs + ']';
        break;

      case STATE_DOWNLOAD_COMPLETE:
      case STATE_EXTRACTING:
      case STATE_APPLYING_DIFFERENTIAL_PATCH:
      case STATE_READY_TO_INSTALL:
        stateDescription = 'Ready to install!';
        extraData = '[Bytes downloaded][' + state.bytesDownloaded + ']' +
                    '[Bytes total][' + state.totalBytesToDownload + ']';

        bundle.install();
        break;

      case STATE_WAITING_TO_INSTALL:
      case STATE_INSTALLING:
        stateDescription = 'Installing...';
        extraData = '[Install Progress][' + state.installProgress + ']' +
                    '[Time remaining][' + state.installTimeRemainingMs + ']';
        break;

      case STATE_INSTALL_COMPLETE:
        stateDescription = 'Done!';
        done = true;
        break;

      case STATE_PAUSED:
        stateDescription = 'Paused...';
        break;

      case STATE_NO_UPDATE:
        stateDescription = 'No update available!';
        done = true;
        break;

      case STATE_ERROR:
        stateDescription = 'Error!';
        extraData = '[errorCode][' + state.errorCode + ']' +
                    '[completionMessage][' + state.completionMessage + ']' +
                    '[installerResultCode][' + state.installerResultCode + ']';
        done = true;
        break;

      default:
        stateDescription = 'Unhandled state...';
        break;
    }

    WScript.Echo('[State][' + state.stateValue + '][' + stateDescription + ']');
    if (extraData.length > 0) {
      WScript.Echo('[Data][' + extraData + ']');
    }

    return done;
  }

  while (!operationDone()) {
    WScript.Sleep(100);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Main
////////////////////////////////////////////////////////////////////////////////

function parseAndRun() {
  if (WScript.Arguments.length < 4) {
    return false;
  }

  var is_chrome_branded = Boolean(parseInt(WScript.Arguments(0)));
  var app_id = WScript.Arguments(1);
  var is_system = Boolean(parseInt(WScript.Arguments(2)));
  var operation = parseInt(WScript.Arguments(3));

  switch (operation) {
    case CHECK_FOR_UPDATE:
      if (WScript.Arguments.length != 4) {
        return false;
      }
      doCheckForUpdate(is_chrome_branded, app_id, is_system);
      break;

    case INSTALL:
      if (WScript.Arguments.length != 4) {
        return false;
      }
      doInstall(is_chrome_branded, app_id, is_system);
      break;

    case UPDATE:
      if (WScript.Arguments.length != 4) {
        return false;
      }
      doUpdate(is_chrome_branded, app_id, is_system);
      break;

    case LAUNCH_COMMAND:
      if (WScript.Arguments.length < 5) {
        return false;
      }
      var command = WScript.Arguments(4);
      var argument_list = [];
      for (var i = 5; i < WScript.Arguments.length; ++i) {
        argument_list.push(WScript.Arguments(i));
      }
      doLaunchCommand(is_chrome_branded,
                      app_id,
                      is_system,
                      command,
                      argument_list);
      break;

    default:
      return false;
  }

  return true;
}

try {
  if (!parseAndRun()) {
    WScript.Echo(
        'Usage: ' +
        '{0|1==Chromium|Chrome Branded} ' +
        '{AppId} ' +
        '{is_system: 0|1} ' +
        '{0|1|2|3==CheckForUpdate|Install|Update|LaunchCommand} ' +
        '{command_id?}');
  }
} catch (ex) {
  if (ex.number == -2147024703) {
    WScript.Echo('ERROR_BAD_EXE_FORMAT: Try the SysWOW64 Script Host: ' +
                 ex.description);
  } else {
    WScript.Echo('Error: ' + ex.description + ': ' + ex.number);
  }
}
