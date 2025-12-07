onmessage = () => {
  var protocol = location.protocol.replace('http', 'ws');
  var url = protocol + '//' + location.host + '/echo-with-no-extension';
  var ws = new WebSocket(url);

  ws.onopen = () => postMessage('PASS');
  ws.onclose = () => postMessage('FAIL');
}
