// Note: This JS can run either in a worker or as a script in the main document
// context.
function connectWebSocketWithMessageCallback(url, messageCallback) {
  var ws = new WebSocket(url);

  ws.onopen = () => { ws.send('hello world'); };

  ws.onclose = () => { messageCallback('onclose'); };

  ws.onmessage = () => { messageCallback('onmessage'); };
}
