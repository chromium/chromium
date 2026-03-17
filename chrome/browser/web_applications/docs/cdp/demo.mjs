import {stdin as input, stdout as output} from 'node:process';
import * as readline from 'node:readline/promises';
import puppeteer from 'puppeteer';
import WebSocket from 'ws';

// Running this test needs a local build with https://crrev.com/c/5532619.
(async () => {
    // Launch the browser and open a new blank page
  const browser = await puppeteer.launch({
    executablePath: 'out/default/chrome'
    headless: false,
    slowMo: 100,
    // Expected to run via WebSocket.
    pipe: false,
  });

  const ws = new WebSocket(browser.wsEndpoint(), {perMessageDeflate: false});
  await new Promise(resolve => ws.once('open', resolve));
  const queue = [];
  var id = 1;

  const send = obj => () => {
    console.log('    >>>> Sending: ' + JSON.stringify(obj));
    ws.send(JSON.stringify({id: id, ...obj}));
  };

  const show_apps =
      () => async () => {
        console.log('**** Now I will show the chrome://apps');
        const page = await browser.newPage();
        await page.goto('chrome://apps');
        run();
      }

  const run = () => {
    if (queue.length > 0) {
      queue.shift()();
    }
  };

  const waitfor_enter = (msg) => async () => {
    const rl = readline.createInterface({input, output});
    await rl.question('**** ' + msg, ans => {
      rl.close();
    });
    run();
  };

  ws.addEventListener('message', e => {
    console.log('    <<<< Result: ' + e.data);
    const res = JSON.parse(e.data);
    if (res.result && res.result.targetId) {
      queue.unshift(send({
        method: 'Target.attachToTarget',
        params: {targetId: res.result.targetId}
      }));
    }
    if (res.id == id) {
      id++;
      run();
    }
  });

  queue.push(send({
    method: 'PWA.launch',
    params: {
      manifestId: 'https://developer.chrome.com/',
    }
  }));

  queue.push(show_apps());

  queue.push(waitfor_enter(
      'The first launch should fail since the app has not been installed yet. Press enter to move forward.'));

  queue.push(send({
    method: 'PWA.install',
    params: {
      manifestId: 'https://developer.chrome.com/',
      installUrlOrBundleUrl: 'https://developer.chrome.com/'
    }
  }));

  queue.push(send({
    method: 'PWA.launch',
    params: {
      manifestId: 'https://developer.chrome.com/',
    }
  }));

  queue.push(waitfor_enter(
      'The second launch should succeed - press enter to move forward'));

  queue.push(send({
    method: 'PWA.getOsAppState',
    params: {
      manifestId: 'https://developer.chrome.com/',
    }
  }));

  // Does not work - it needs a WebSocket connecting to the page.
  queue.push(send({
    method: 'Page.getAppManifest',
    params: {
      manifestId: 'https://developer.chrome.com/',
    }
  }));

  queue.push(show_apps());

  queue.push(waitfor_enter('Press enter to uninstall the web app.'));

  queue.push(send({
    method: 'PWA.uninstall',
    params: {
      manifestId: 'https://developer.chrome.com/',
    }
  }));

  queue.push(
      waitfor_enter('Press enter to close the browser window and stop.'));

  queue.push(() => {
    if (queue.length > 0) {
      throw new Error('The queue should be empty now.')
    }
    browser.close();
    process.exit();
  });

  run();
})();
