import * as common from './common.mjs';
Object.assign(global, common);

(async () => {
  const default_param = {
    manifestId: 'https://developer.chrome.com/'
  };

  await waitfor_enter('Now I will show the chrome://apps, there should not ' +
                      'be any apps so far.');
  await (await browser.newPage()).goto('chrome://apps');
  await waitfor_enter('The first launch should fail since the app has not ' +
                      'been installed yet.');
  await send(null, 'PWA.launch', default_param);

  await waitfor_enter('Now I will install the web app.');
  await send(null, 'PWA.install', {
    manifestId: 'https://developer.chrome.com/',
    installUrlOrBundleUrl: 'https://developer.chrome.com/'
  });

  await waitfor_enter('You should see the newly installed app in the ' +
                      'chrome://apps tab now. It will be launched.');
  await send(null, 'PWA.launch', default_param);
  await waitfor_enter('The second launch should succeed. Now I will inspect ' +
                      'its information.');
  await send(await current_page_session(),
             'Page.getAppManifest',
             default_param);
  await send(null, 'PWA.getOsAppState', default_param);
  await waitfor_enter('Now I will open the page in its own window.');
  await send(await current_page_session(),
             'PWA.openCurrentPageInApp',
             default_param);
  await waitfor_enter('Now I will close the page and uninstall the webapp.');
  await (await browser.pages()).pop().close();
  await send(null, 'PWA.uninstall', default_param);
  await waitfor_enter('You should see the installed app being removed in the ' +
                      'chrome://apps tab now. Will close the browser window ' +
                      'and stop.');
  browser.close();
  shutdown();
})();
