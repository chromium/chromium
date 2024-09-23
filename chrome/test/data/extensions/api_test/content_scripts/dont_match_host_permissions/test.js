chrome.runtime.sendMessage({
  source: location.hostname,
  modified: window.title == 'Hello'
});
