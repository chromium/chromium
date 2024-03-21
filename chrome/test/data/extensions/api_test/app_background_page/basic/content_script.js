var scriptMessageEvent = document.createEvent("Event");
scriptMessageEvent.initEvent('scriptMessage', true, true);

var pageToScriptTunnel = document.getElementById("pageToScriptTunnel");
pageToScriptTunnel.addEventListener("scriptMessage", function() {
  var data = JSON.parse(pageToScriptTunnel.innerText);
  chrome.runtime.sendMessage(data);
});

chrome.runtime.onMessage.addListener(function(request) {
  var scriptToPageTunnel = document.getElementById("scriptToPageTunnel");
  scriptToPageTunnel.innerText = JSON.stringify(request);
  scriptToPageTunnel.dispatchEvent(scriptMessageEvent);
});
