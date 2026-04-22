
chrome.browserAction.onClicked.addListener(function() {
  const url = chrome.runtime.getURL('missing.txt');
  console.info('trying to fetch ' + url);
  const xhr = new XMLHttpRequest();
  xhr.open('GET', url);
  xhr.send();
  xhr.onReadyStateChange = function() {
    console.info('onReadyStateChange : ' + xhr.readyState);
    // eslint-disable-next-line no-console
    console.dir(xhr);
  };
  xhr.onerror = function() {
    console.info('error');
    // eslint-disable-next-line no-console
    console.dir(xhr);
  };
});
